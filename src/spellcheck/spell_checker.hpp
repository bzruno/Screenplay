#pragma once
// spellcheck/spell_checker.hpp
// Windows SpellCheck API (ISpellChecker) with multi-language support.
// A word is misspelled only when ALL selected language checkers flag it
// (so words from one language are not flagged by another checker).
// Falls back gracefully to "no spell check" if COM init fails.

#include <string>
#include <vector>
#include <algorithm>

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <spellcheck.h>
#  include <wrl/client.h>
#  include <objbase.h>
#  pragma comment(lib, "ole32.lib")
#endif

namespace screenplay::spellcheck {

struct Misspelling {
    size_t start;
    size_t length;
    std::vector<std::string> suggestions;
};

// ── SEH wrappers (free functions — no C++ objects with destructors) ──────

#ifdef _WIN32

static bool seh_try_create_spell_factory(void** out_factory) {
    __try {
        HRESULT hr = CoCreateInstance(
            __uuidof(SpellCheckerFactory), nullptr,
            CLSCTX_INPROC_SERVER,
            __uuidof(ISpellCheckerFactory),
            out_factory);
        return SUCCEEDED(hr);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static bool seh_try_check(ISpellChecker* checker, const wchar_t* text,
                           IEnumSpellingError** out_errors) {
    __try {
        HRESULT hr = checker->Check(text, out_errors);
        return SUCCEEDED(hr);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static bool seh_try_suggest(ISpellChecker* checker, const wchar_t* word,
                             IEnumString** out_enum) {
    __try {
        HRESULT hr = checker->Suggest(word, out_enum);
        return SUCCEEDED(hr);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static bool seh_try_add(ISpellChecker* checker, const wchar_t* word) {
    __try {
        checker->Add(word);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

#endif

class SpellChecker {
public:
    SpellChecker() {
#ifdef _WIN32
        init_com();
        init_checkers({"en-US"});   // default until reinit() is called
#endif
    }

    ~SpellChecker() {
#ifdef _WIN32
        checkers_.clear();
        if (com_ok_) CoUninitialize();
#endif
    }

    SpellChecker(const SpellChecker&) = delete;
    SpellChecker& operator=(const SpellChecker&) = delete;

    bool available() const { return available_; }

    // Replace checker set with the supplied language tags (e.g. "en-US", "pt-BR").
    // Safe to call at any time after construction.
    void reinit(const std::vector<std::string>& lang_tags) {
#ifdef _WIN32
        checkers_.clear();
        available_ = false;
        if (!com_ok_ || lang_tags.empty()) return;
        init_checkers(lang_tags);
#endif
    }

    // A word is misspelled only when ALL active checkers flag it.
    // Suggestions are merged from all checkers that flagged the word.
    std::vector<Misspelling> check(const std::string& text) const {
        std::vector<Misspelling> result;
        if (!available_ || text.empty()) return result;
#ifdef _WIN32
        result = check_multi(text);
#endif
        return result;
    }

    void add_to_dictionary(const std::string& word) {
        if (!available_ || word.empty()) return;
#ifdef _WIN32
        std::wstring wword = utf8_to_wide(word);
        for (auto& ch : checkers_)
            seh_try_add(ch.Get(), wword.c_str());
#endif
    }

private:
    bool available_ = false;

#ifdef _WIN32
    bool com_ok_ = false;
    std::vector<Microsoft::WRL::ComPtr<ISpellChecker>> checkers_;

    void init_com() {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return;
        com_ok_ = true;
    }

    void init_checkers(const std::vector<std::string>& lang_tags) {
        ISpellCheckerFactory* raw_factory = nullptr;
        if (!seh_try_create_spell_factory(reinterpret_cast<void**>(&raw_factory)))
            return;
        Microsoft::WRL::ComPtr<ISpellCheckerFactory> factory;
        factory.Attach(raw_factory);

        for (const auto& tag : lang_tags) {
            std::wstring wtag = utf8_to_wide(tag);
            BOOL supported = FALSE;
            HRESULT hr = factory->IsSupported(wtag.c_str(), &supported);
            if (FAILED(hr) || !supported) continue;
            Microsoft::WRL::ComPtr<ISpellChecker> checker;
            hr = factory->CreateSpellChecker(wtag.c_str(), &checker);
            if (SUCCEEDED(hr) && checker) {
                checkers_.push_back(checker);
                available_ = true;
            }
        }
        if (available_) add_screenplay_terms();
    }

    // Check with one language checker.
    std::vector<Misspelling> check_one(
        const Microsoft::WRL::ComPtr<ISpellChecker>& checker,
        const std::string& text) const
    {
        std::vector<Misspelling> result;
        std::wstring wtext = utf8_to_wide(text);
        if (wtext.empty()) return result;

        IEnumSpellingError* raw_errors = nullptr;
        if (!seh_try_check(checker.Get(), wtext.c_str(), &raw_errors)) return result;
        Microsoft::WRL::ComPtr<IEnumSpellingError> errors;
        errors.Attach(raw_errors);
        if (!errors) return result;

        Microsoft::WRL::ComPtr<ISpellingError> err;
        while (errors->Next(&err) == S_OK) {
            ULONG start = 0, len = 0;
            err->get_StartIndex(&start);
            err->get_Length(&len);

            size_t byte_start = wide_pos_to_utf8(text, wtext, start);
            size_t byte_len   = wide_pos_to_utf8(text, wtext, start + len) - byte_start;

            Misspelling ms;
            ms.start  = byte_start;
            ms.length = byte_len;

            IEnumString* raw_sugg = nullptr;
            if (seh_try_suggest(checker.Get(),
                                wtext.substr(start, len).c_str(), &raw_sugg)
                && raw_sugg)
            {
                Microsoft::WRL::ComPtr<IEnumString> sugg_enum;
                sugg_enum.Attach(raw_sugg);
                LPOLESTR sugg = nullptr;
                ULONG fetched = 0;
                int count = 0;
                while (sugg_enum->Next(1, &sugg, &fetched) == S_OK && count < 5) {
                    ms.suggestions.push_back(wide_to_utf8(sugg));
                    CoTaskMemFree(sugg);
                    ++count;
                }
            }

            result.push_back(std::move(ms));
            err.Reset();
        }
        return result;
    }

    // Intersection: only keep words flagged by ALL checkers.
    // Suggestions are merged from every checker that agreed.
    std::vector<Misspelling> check_multi(const std::string& text) const {
        if (checkers_.empty()) return {};
        auto result = check_one(checkers_[0], text);
        for (size_t i = 1; i < checkers_.size(); ++i) {
            auto other = check_one(checkers_[i], text);
            std::vector<Misspelling> kept;
            for (auto& ms : result) {
                for (auto& oms : other) {
                    if (oms.start == ms.start && oms.length == ms.length) {
                        // Merge unique suggestions from this checker
                        for (const auto& s : oms.suggestions) {
                            bool dup = false;
                            for (const auto& es : ms.suggestions)
                                if (es == s) { dup = true; break; }
                            if (!dup) ms.suggestions.push_back(s);
                        }
                        kept.push_back(std::move(ms));
                        break;
                    }
                }
            }
            result = std::move(kept);
        }
        return result;
    }

    void add_screenplay_terms() {
        static const char* terms[] = {
            "INT", "EXT", "FADE", "CUT", "DISSOLVE",
            "CONTINUA", "CONT'D", "V.O", "O.S", "O.F",
            "DIA", "NOITE", "TARDE", "MANH\xc3\x83", "ENTARDECER",
            "MADRUGADA", "AMANHECER",
            "ROTEIRO", "CENA", "PERSONAGEM", "DI\xc3\x81LOGO",
            "\xc3\x87\xc3\x83O", "TRANSI\xc3\x87\xc3\x83O", "PAR\xc3\x8aNTESE",
            nullptr
        };
        for (int i = 0; terms[i]; ++i)
            add_to_dictionary(terms[i]);
    }

    // ── UTF-8 <-> wide helpers ─────────────────────────────────────────────

    static std::wstring utf8_to_wide(const std::string& s) {
        if (s.empty()) return {};
        int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
        if (n <= 0) return {};
        std::wstring w(n, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
        return w;
    }

    static std::string wide_to_utf8(const std::wstring& w) {
        if (w.empty()) return {};
        int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(),
                                    nullptr, 0, nullptr, nullptr);
        if (n <= 0) return {};
        std::string s(n, '\0');
        WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(),
                            s.data(), n, nullptr, nullptr);
        return s;
    }

    static std::string wide_to_utf8(const wchar_t* w) {
        if (!w) return {};
        return wide_to_utf8(std::wstring(w));
    }

    static size_t wide_pos_to_utf8(const std::string& utf8,
                                    const std::wstring& /*wide*/,
                                    ULONG wpos)
    {
        size_t byte_idx = 0;
        ULONG  wchar_count = 0;
        while (byte_idx < utf8.size() && wchar_count < wpos) {
            unsigned char c = static_cast<unsigned char>(utf8[byte_idx]);
            size_t seq_len = 1;
            if      (c < 0x80) seq_len = 1;
            else if (c < 0xE0) seq_len = 2;
            else if (c < 0xF0) seq_len = 3;
            else               seq_len = 4;
            wchar_count += (seq_len == 4) ? 2 : 1;
            byte_idx += seq_len;
        }
        return (std::min)(byte_idx, utf8.size());
    }
#endif
};

} // namespace screenplay::spellcheck

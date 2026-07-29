# Guia de Instalação — Windows 11

## Método 1: Script automático (recomendado)

Abra o **PowerShell como Administrador** e rode:

```powershell
Set-ExecutionPolicy RemoteSigned -Scope CurrentUser
cd caminho\para\screenplay_editor
.\install_windows.ps1
```

O script instala tudo automaticamente. Vai demorar **30–60 min** na primeira vez (Qt é grande).

---

## Método 2: Passo a passo manual

### Passo 1 — Visual Studio 2022

1. Baixe: https://visualstudio.microsoft.com/downloads/
2. Escolha **"Build Tools for Visual Studio 2022"** (gratuito)
3. No instalador, marque:
   - ✅ **Desktop development with C++**
   - ✅ **Windows 11 SDK (10.0.22621)**

### Passo 2 — CMake

Baixe e instale: https://cmake.org/download/  
Marque **"Add CMake to system PATH"** durante a instalação.

### Passo 3 — Git

Baixe: https://git-scm.com/download/win

### Passo 4 — vcpkg

Abra o **PowerShell** (não precisa ser Admin):

```powershell
git clone https://github.com/microsoft/vcpkg C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat -disableMetrics
```

### Passo 5 — Qt6 + FreeType

```powershell
C:\vcpkg\vcpkg.exe install qt6-base:x64-windows freetype:x64-windows
```

⏳ Isso demora **20–60 minutos** na primeira vez.

### Passo 6 — Compilar

Abra o **"Developer Command Prompt for VS 2022"** (pesquise no menu Iniciar):

```bat
cd C:\caminho\para\screenplay_editor

cmake -B build ^
  -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake ^
  -DCMAKE_BUILD_TYPE=Release ^
  -A x64

cmake --build build --config Release --parallel
```

### Passo 7 — Deploy das DLLs do Qt

```bat
C:\vcpkg\installed\x64-windows\tools\Qt6\bin\windeployqt.exe ^
  --release build\Release\ScreenplayEditor.exe
```

### Passo 8 — Executar

```bat
build\Release\ScreenplayEditor.exe
```

---

## Compilar os testes

```bat
cmake --build build --target ScreenplayTests --config Release
build\Release\ScreenplayTests.exe
```

Saída esperada:
```
Screenplay Editor — Unit Tests
========================================
  PASS  s.blocks.empty()
  PASS  b.type == BlockType::SceneHeading
  ...
Results: 99 passed, 0 failed.
```

---

## Solução de problemas

### "CMake can't find Qt6"
```powershell
cmake -B build -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake -DCMAKE_PREFIX_PATH=C:\vcpkg\installed\x64-windows
```

### "FreeType not found"
```powershell
C:\vcpkg\vcpkg.exe install freetype:x64-windows --recurse
```

### O programa abre mas o texto não aparece
- Verifique se `C:\Windows\Fonts\CourierNew.ttf` existe
- Se não existir, instale a fonte Courier New pelo Windows ou coloque `CourierPrime-Regular.ttf` na pasta `fonts/` do projeto
- Download grátis: https://quoteunquoteapps.com/courierprime/

### "VCRUNTIME140.dll not found"
Instale o Visual C++ Redistributable:
https://aka.ms/vs/17/release/vc_redist.x64.exe

---

## Estrutura após compilação

```
build\Release\
├── ScreenplayEditor.exe    ← programa principal
├── ScreenplayTests.exe     ← testes unitários
├── Qt6Core.dll
├── Qt6Gui.dll
├── Qt6Widgets.dll
├── freetype.dll
└── platforms\
    └── qwindows.dll
```

---

## Atalhos de teclado

| Tecla          | Ação                              |
|----------------|-----------------------------------|
| Enter          | Avança tipo de bloco              |
| Tab            | Cicla tipo de bloco (frente)      |
| Shift+Tab      | Cicla tipo de bloco (trás)        |
| Ctrl+Z         | Desfazer                          |
| Ctrl+Shift+Z   | Refazer                           |
| Ctrl+S         | Salvar                            |
| Ctrl+Shift+S   | Salvar como                       |
| Ctrl+O         | Abrir                             |
| Ctrl+N         | Novo                              |
| Ctrl+= / Ctrl+-| Zoom in / out                     |
| Ctrl+0         | Reset zoom                        |
| Ctrl+Scroll    | Zoom com o mouse                  |

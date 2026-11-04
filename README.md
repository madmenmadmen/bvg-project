# BVG - Binary Vector Graphics

BVG - это компактный бинарный формат для 2D-векторной графики. Он хранит геометрию, стили, градиенты, палитры и дерево групп в детерминированном chunk-ориентированном файле.

[![BVG WASM Viewer](docs/neon_preview.png)](https://madmenmadmen.github.io/bvg-project/?raw=true&file=neon.bvg)

## Реализации

| Язык | Библиотека | CLI | Вьювер |
|------|-----------|-----|--------|
| Python | bvg-py | да | PyQt6 |
| Java | bvg-java | да | JavaFX + Java2D |
| C++ | bvg-cpp | да | Qt6 |
| Rust | bvg-rust | да | нет |
| Rust + Qt | bvg-rust-qt | нет | Qt6 + FFI |
| Rust + WASM | bvg-wasm | нет | Canvas 2D |

## Conformance

Все 5 реализаций проходят 16/16 тест-векторов:

    $ python conformance.py --impl bvg-py,bvg-java,bvg-cpp,bvg-rust,bvg-wasm
    80/80 ok

## Сборка

Репозиторий содержит только исходный код. Зависимости для C++ (zlib, zstd, lz4) включены в third_party.

1. Соберите все реализации:

    $ build_all.cmd

2. Пересоберите WASM для Node.js (нужно для conformance):

    $ cd bvg-wasm
    $ wasm-pack build --target nodejs --out-dir node/pkg

3. Запустите тесты:

    $ python conformance.py --impl bvg-py,bvg-java,bvg-cpp,bvg-rust,bvg-wasm

Требования: CMake, Visual Studio 2022 (MSVC), Qt6, Gradle, Cargo, wasm-pack, Python 3.10+.

## Спецификация

Полная спецификация формата доступна в отдельном репозитории: [bvg-spec](https://github.com/madmenmadmen/bvg-spec).

## Лицензия

MIT
# cpp-json-store

Almacén de documentos JSON con respaldo SQLite — herramienta CLI en C++23.

## Características

- Almacenamiento y recuperación de documentos JSON con clave única
- Búsqueda de texto completo en documentos y claves
- Listado con filtro por prefijo
- Importación/exportación de colecciones JSON
- Operaciones CRUD completas desde línea de comandos
- Persistencia SQLite con WAL mode para concurrencia
- Logging estructurado con spdlog
- Cobertura de tests con Catch2

## Requisitos

- CMake 3.28+
- Compilador C++23 (GCC 14+ / Clang 18+)
- SQLite3 (development headers)
- Ninja (opcional, recomendado)

## Compilar

```bash
cmake --preset debug
cmake --build --preset debug
```

## Uso

```bash
# Almacenar un documento
./build/debug/jsonstore put --key "user:1" --value '{"name":"Alice","role":"admin"}'

# Recuperar un documento
./build/debug/jsonstore get --key "user:1"

# Listar documentos con prefijo
./build/debug/jsonstore list --prefix "user:"

# Buscar documentos
./build/debug/jsonstore search --search "Alice"

# Contar documentos
./build/debug/jsonstore count

# Importar desde archivo JSON
./build/debug/jsonstore import --file data.json

# Exportar todos los documentos
./build/debug/jsonstore export
```

## Tests

```bash
cd build/debug && ctest --output-on-failure
```

## Licencia

MIT

# dbgen4

database access layer generator

## build debepndencies

### magic enum

  sudo dnf install magic_enum-devel

### SPD log

   sudo dnf install spdlog-devel.x86_64 spdlog.x86_64

### fmt library

   sudo dnf install fmt.x86_64

### magic_enum

   sudo dnf install magic_enum-devel.x86_64

### nlohmann json

   sudo dnf install json-devel

### json-schema-validator

   git clone <https://github.com/pboettch/json-schema-validator.git>
   cd json-schema-validator
   mkdir build
   cd build
   cmake [..]
   make
   make install

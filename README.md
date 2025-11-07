# dbgen4

database access layer generator

## build debepndencies

### yaml-cpp

   sudo dnf install yaml-cpp-devel

### cli11 comand line parsing library

   sudo dnf install cli11-devel

### sanitation library

   sudo dnf install libasan libubsan

### magic enum

  sudo dnf install magic_enum-devel

### SPD log

   sudo dnf install spdlog-devel.x86_64 spdlog.x86_64

### fmt library

   sudo dnf install fmt.x86_64

### ODBC drive instalation

   sudo dnf install unixODBC-devel

### nlohmann

   sudo dnf install nlohmann-json-devel

### valijson

   sudo dnf install valijson-devel

## database installations

### mariadb

   sudo dnf install mariadb-server mariadb-client
   sudo mysql_secure_installation
   sudo dnf install unixODBC
   sudo dnf install mariadb-connector-odbc
   <https://mariadb.com/kb/en/mariadb-connector-odbc/>

   start mariadb

   sudo service mariadb start

## Doxygen

   sudo dnf install doxygen mscgen dia

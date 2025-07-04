cmake_minimum_required(VERSION 3.10)

project(TradeBotC)

# Указываем toolchain для vcpkg
set(CMAKE_TOOLCHAIN_FILE "C:/Users/Chay/source/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")

# Находим другие зависимости
find_package(fmt CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS beast asio)
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)  # Добавляем OpenSSL

# Указываем путь к MySQL Connector/C++
set(MYSQL_CONNECTOR_CPP_ROOT "D:/Programs/mysql-c++-9.2.0")

# Добавьте заголовочные файлы
include_directories(${MYSQL_CONNECTOR_CPP_ROOT}/include)

# Добавьте библиотеки (указываем путь к библиотеке в зависимости от конфигурации)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    link_directories(${MYSQL_CONNECTOR_CPP_ROOT}/lib64/debug)
else()
    link_directories(${MYSQL_CONNECTOR_CPP_ROOT}/lib64)
endif()

# Создаем исполнимый файл
add_executable(TradeBotC "src/TradeBotC.cpp" "src/Informers/Informer.hpp" "src/Informers/Crypto/ByBitInformer.hpp" "src/strategy_registrations.hpp")

# Подключаем зависимости
target_link_libraries(
    TradeBotC PRIVATE
    Boost::beast
    Boost::asio
    fmt::fmt
    CURL::libcurl
    OpenSSL::SSL  # Подключаем OpenSSL
    OpenSSL::Crypto  # Подключаем OpenSSL
    mysqlcppconn  # Указываем библиотеку MySQL Connector/C++
)
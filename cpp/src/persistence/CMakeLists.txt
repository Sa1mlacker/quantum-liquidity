add_library(persistence OBJECT
    postgres_writer.cpp
    redis_client.cpp
    time_series_db.cpp
)

target_include_directories(persistence PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(persistence PUBLIC
    common
    # TODO: libpq, hiredis
)

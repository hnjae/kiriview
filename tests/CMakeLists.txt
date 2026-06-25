add_executable(tst_imageviewport
    tst_imageviewport.cpp
)

target_link_libraries(tst_imageviewport
    PRIVATE
        ImageViewport
        Qt6::Quick
        Qt6::Test
)

add_test(NAME imageviewport COMMAND tst_imageviewport)

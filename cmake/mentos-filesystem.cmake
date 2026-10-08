# =============================================================================
# ROOT FILESYSTEM IMAGE
# =============================================================================

# MentOS is compatible with EXT2 filesystems. This target generates an EXT2
# filesystem using the content of the `filesystem` folder.
add_custom_target(filesystem
    BYPRODUCTS ${CMAKE_BINARY_DIR}/rootfs.img
    COMMAND echo '============================================================================='
    COMMAND echo 'Creating EXT2 filesystem...'
    COMMAND echo '============================================================================='
    COMMAND ${MKE2FS_EXE} -L 'rootfs' -N 0 -d ${CMAKE_SOURCE_DIR}/filesystem -b 4096 -m 5 -r 1 -t ext2 -v -F ${CMAKE_BINARY_DIR}/rootfs.img 32M
    # Plant the deep-path symlink fixture for t_symlink (#288) inside the
    # image: its path is too long to stage through the host filesystem.
    COMMAND ${CMAKE_COMMAND} -DROOTFS=${CMAKE_BINARY_DIR}/rootfs.img -DBINARY_DIR=${CMAKE_BINARY_DIR} -P ${CMAKE_SOURCE_DIR}/cmake/make-symlink-fixture.cmake
    COMMAND echo '============================================================================='
    COMMAND echo 'Done!'
    COMMAND echo '============================================================================='
    DEPENDS programs tests
)

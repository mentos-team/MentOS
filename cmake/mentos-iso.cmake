# =============================================================================
# GRUB ISO PACKAGING
# =============================================================================

# Build one GRUB ISO from a selected configuration. Keeping staging and
# packaging in one helper prevents the normal, userspace-test, and kernel-test
# images from drifting apart when the boot artifact layout changes.
function(mentos_add_grub_iso target_name output_name staging_name grub_config)
    set(staging_dir ${CMAKE_BINARY_DIR}/${staging_name})
    set(grub_config_source ${CMAKE_SOURCE_DIR}/iso/boot/grub/${grub_config})

    add_custom_target(
        ${target_name}
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${staging_dir}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/iso ${staging_dir}
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/mentos/bootloader.bin ${staging_dir}/boot/bootloader.bin
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/mentos/kernel.bin ${staging_dir}/boot/kernel.bin
        COMMAND ${CMAKE_COMMAND} -E copy ${grub_config_source} ${staging_dir}/boot/grub/grub.cfg
        COMMAND "${GRUB_MKRESCUE_EXE}" -o ${CMAKE_BINARY_DIR}/${output_name} ${staging_dir}
        DEPENDS bootloader.bin kernel.bin ${ARGN}
    )
endfunction()

mentos_add_grub_iso(cdrom.iso cdrom.iso iso grub.cfg)
mentos_add_grub_iso(cdrom_test.iso cdrom_test.iso iso_test grub.cfg.runtests filesystem)
mentos_add_grub_iso(cdrom_kerneltest.iso cdrom_kerneltest.iso iso_kerneltest grub.cfg.kerneltests filesystem)

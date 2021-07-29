sdbuscpp = find_program('sdbus++')

firmware_update_server_hpp = custom_target(
                'server.hpp',
                command : [
                    sdbuscpp, '-r', meson.source_root(),
                    'interface',
                    'server-header',
                    'xyz.openbmc_project.Software.FirmwareUpdate',
                ],
                input : '../FirmwareUpdate.interface.yaml',
                capture : true,
                output : 'server.hpp',
                install : true,
                install_dir: get_option('includedir') / 'xyz/openbmc_project/Software/FirmwareUpdate',
)

firmware_update_server_cpp = custom_target(
                'server.cpp',
                command : [
                    sdbuscpp, '-r', meson.source_root(),
                    'interface',
                    'server-cpp',
                    'xyz.openbmc_project.Software.FirmwareUpdate'
                ],
                input : '../FirmwareUpdate.interface.yaml',
                depends : firmware_update_server_hpp,
                capture : true,
                output : 'server.cpp',
                install : true,
                install_dir: get_option('includedir') / 'xyz/openbmc_project/Software/FirmwareUpdate',
)

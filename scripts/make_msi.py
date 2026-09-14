#!/usr/bin/env python3
import os
import sys
import argparse
import subprocess
import xml.etree.ElementTree as ET

def generate_msi(bundle_dir, output_msi, icon_path=None, wixl_bin="wixl", lib_dir=None):
    bundle_dir = os.path.abspath(bundle_dir)
    output_msi = os.path.abspath(output_msi)

    wix = ET.Element('Wix', {'xmlns': 'http://schemas.microsoft.com/wix/2006/wi'})
    prod = ET.SubElement(wix, 'Product', {
        'Id': '*',
        'Name': 'Andub',
        'Language': '1033',
        'Version': '1.0.0',
        'Manufacturer': 'Andub Team',
        'UpgradeCode': '6b595ec3-6316-4d92-bb89-7cf8b9821234'
    })
    ET.SubElement(prod, 'Package', {
        'InstallerVersion': '200',
        'Compressed': 'yes',
        'InstallScope': 'perMachine'
    })
    ET.SubElement(prod, 'MediaTemplate', {'EmbedCab': 'yes'})

    if icon_path and os.path.exists(icon_path):
        icon_path = os.path.abspath(icon_path)
        ET.SubElement(prod, 'Icon', {'Id': 'AppIcon', 'SourceFile': icon_path})
        ET.SubElement(prod, 'Property', {'Id': 'ARPPRODUCTICON', 'Value': 'AppIcon'})

    # Directories
    target_dir = ET.SubElement(prod, 'Directory', {'Id': 'TARGETDIR', 'Name': 'SourceDir'})
    pf = ET.SubElement(target_dir, 'Directory', {'Id': 'ProgramFiles64Folder'})
    install_dir = ET.SubElement(pf, 'Directory', {'Id': 'INSTALLDIR', 'Name': 'Andub'})

    prog_menu = ET.SubElement(target_dir, 'Directory', {'Id': 'ProgramMenuFolder'})
    app_prog_folder = ET.SubElement(prog_menu, 'Directory', {'Id': 'ApplicationProgramsFolder', 'Name': 'Andub'})
    desktop_folder = ET.SubElement(target_dir, 'Directory', {'Id': 'DesktopFolder', 'Name': 'Desktop'})

    # Shortcut component
    app_sc_cmp = ET.SubElement(app_prog_folder, 'Component', {'Id': 'AppShortcuts', 'Guid': '*', 'Win64': 'yes'})
    
    # Start Menu Shortcut
    sc_start_attrs = {
        'Id': 'AppStartMenuShortcut',
        'Name': 'Andub',
        'Description': 'Andub Anime Player',
        'Target': '[INSTALLDIR]andub.exe',
        'WorkingDirectory': 'INSTALLDIR'
    }
    if icon_path and os.path.exists(icon_path):
        sc_start_attrs['Icon'] = 'AppIcon'
        sc_start_attrs['IconIndex'] = '0'
    ET.SubElement(app_sc_cmp, 'Shortcut', sc_start_attrs)

    # Desktop Shortcut
    sc_desk_attrs = {
        'Id': 'AppDesktopShortcut',
        'Directory': 'DesktopFolder',
        'Name': 'Andub',
        'Description': 'Andub Anime Player',
        'Target': '[INSTALLDIR]andub.exe',
        'WorkingDirectory': 'INSTALLDIR'
    }
    if icon_path and os.path.exists(icon_path):
        sc_desk_attrs['Icon'] = 'AppIcon'
        sc_desk_attrs['IconIndex'] = '0'
    ET.SubElement(app_sc_cmp, 'Shortcut', sc_desk_attrs)

    ET.SubElement(app_sc_cmp, 'RemoveFolder', {'Id': 'CleanUpShortCut', 'On': 'uninstall'})
    ET.SubElement(app_sc_cmp, 'RegistryValue', {
        'Root': 'HKCU',
        'Key': r'Software\Andub\Andub',
        'Name': 'installed',
        'Type': 'integer',
        'Value': '1',
        'KeyPath': 'yes'
    })

    feature = ET.SubElement(prod, 'Feature', {'Id': 'Complete', 'Level': '1'})
    ET.SubElement(feature, 'ComponentRef', {'Id': 'AppShortcuts'})

    # Directory map: relative path -> XML Directory Element
    dir_map = {'': install_dir}
    comp_idx = 0
    file_idx = 0

    for root, dirs, files in os.walk(bundle_dir):
        rel_root = os.path.relpath(root, bundle_dir)
        if rel_root == '.':
            rel_root = ''
        
        current_dir_el = dir_map[rel_root]

        for d in sorted(dirs):
            rel_sub = os.path.join(rel_root, d) if rel_root else d
            dir_id = f'dir_{abs(hash(rel_sub))}_{len(dir_map)}'
            sub_el = ET.SubElement(current_dir_el, 'Directory', {'Id': dir_id, 'Name': d})
            dir_map[rel_sub] = sub_el

        if files:
            cmp_id = f'cmp_{comp_idx}'
            comp_idx += 1
            cmp_el = ET.SubElement(current_dir_el, 'Component', {'Id': cmp_id, 'Guid': '*', 'Win64': 'yes'})
            ET.SubElement(feature, 'ComponentRef', {'Id': cmp_id})
            for f in sorted(files):
                file_id = f'fil_{file_idx}'
                file_idx += 1
                src_path = os.path.join(root, f)
                ET.SubElement(cmp_el, 'File', {
                    'Id': file_id,
                    'Source': src_path,
                    'Name': f,
                    'KeyPath': 'yes' if f == files[0] else 'no'
                })

    wxs_path = os.path.join(os.path.dirname(output_msi), 'andub_installer.wxs')
    ET.ElementTree(wix).write(wxs_path, encoding='utf-8', xml_declaration=True)
    print(f"[MSI] WiX source generated: {wxs_path} ({file_idx} files in {comp_idx} components)")

    env = os.environ.copy()
    if lib_dir:
        curr_lp = env.get('LD_LIBRARY_PATH', '')
        env['LD_LIBRARY_PATH'] = f"{lib_dir}:{curr_lp}" if curr_lp else lib_dir

    print(f"[MSI] Executing wixl to build: {output_msi}...")
    cmd = [wixl_bin, '-a', 'x64', '-o', output_msi, wxs_path]
    res = subprocess.run(cmd, env=env, capture_output=True, text=True)

    if res.returncode != 0:
        print("[MSI] wixl failed:")
        print(res.stderr)
        sys.exit(res.returncode)

    if os.path.exists(output_msi):
        size_mb = os.path.getsize(output_msi) / (1024 * 1024)
        print(f"[MSI] Successfully created installer: {output_msi} ({size_mb:.2f} MB)")

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Create Andub Windows MSI installer using wixl")
    parser.add_argument('--bundle-dir', required=True, help="Directory containing the Windows distribution bundle")
    parser.add_argument('--output-msi', required=True, help="Path for output .msi file")
    parser.add_argument('--icon', help="Path to .ico application icon")
    parser.add_argument('--wixl', default="wixl", help="Path to wixl binary")
    parser.add_argument('--lib-dir', help="Directory containing libmsi.so / libgcab.so")

    args = parser.parse_args()
    generate_msi(args.bundle_dir, args.output_msi, args.icon, args.wixl, args.lib_dir)

#!/usr/bin/env python3

import yaml

def generate_clang_tidy():
    with open('.clangd', 'r') as f:
        config = yaml.safe_load(f)
    
    if 'Diagnostics' not in config or 'ClangTidy' not in config['Diagnostics']:
        print("No ClangTidy configuration found in .clangd")
        return

    clang_tidy = config['Diagnostics']['ClangTidy']
    
    # Write configuration
    with open('.clang-tidy', 'w') as f:
        f.write('---\n')
        
        # Collect and write checks
        checks = []
        checks.extend(clang_tidy.get('Add', []))
        checks.extend(f'-{check}' for check in clang_tidy.get('Remove', []))
        f.write(f"Checks: '{','.join(checks)}'\n")
        
        # Write standard settings
        f.write('WarningsAsErrors: ""\n')
        f.write('HeaderFilterRegex: ""\n')
        f.write('FormatStyle: file\n')

        # Ensure C++23 settings are present
        check_options = clang_tidy.get('CheckOptions', {}).copy()
        if 'cppcoreguidelines.LanguageStandard' not in check_options:
            check_options['cppcoreguidelines.LanguageStandard'] = 'c++23'
        if 'modernize.LanguageStandard' not in check_options:
            check_options['modernize.LanguageStandard'] = 'c++23'

        # Write CheckOptions
        if check_options:
            f.write('CheckOptions:\n')
            for key, value in sorted(check_options.items()):
                # Handle different value types (string, bool, int)
                if isinstance(value, bool):
                    value_str = str(value).lower()
                elif isinstance(value, (int, float)):
                    value_str = str(value)
                else:
                    value_str = f'"{value}"'
                f.write(f'  - key: {key}\n')
                f.write(f'    value: {value_str}\n')

        # Verify C++23 flag in CompileFlags
        compile_flags = config.get('CompileFlags', {}).get('Add', [])
        if not any('-std=c++23' in flag for flag in compile_flags):
            print("Warning: -std=c++23 not found in CompileFlags. Consider adding it to .clangd")

if __name__ == '__main__':
    generate_clang_tidy()

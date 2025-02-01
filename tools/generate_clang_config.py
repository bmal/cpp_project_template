#!/usr/bin/env python3

import yaml

def generate_clang_tidy():
    with open('.clangd', 'r') as f:
        config = yaml.safe_load(f)
    
    if 'Diagnostics' not in config or 'ClangTidy' not in config['Diagnostics']:
        print("No ClangTidy configuration found in .clangd")
        return

    clang_tidy = config['Diagnostics']['ClangTidy']
    
    # Collect checks
    checks = []
    checks.extend(clang_tidy.get('Add', []))
    checks.extend(f'-{check}' for check in clang_tidy.get('Remove', []))

    # Write configuration
    with open('.clang-tidy', 'w') as f:
        f.write('---\n')
        f.write(f"Checks: '{','.join(checks)}'\n")
        f.write('WarningsAsErrors: ""\n')
        f.write('HeaderFilterRegex: ""\n')
        f.write('FormatStyle: file\n')

        
        # Add CheckOptions if present
        if 'CheckOptions' in clang_tidy:
            f.write('CheckOptions:\n')
            for key, value in sorted(clang_tidy['CheckOptions'].items()):
                # Handle different value types (string, bool, int)
                if isinstance(value, bool):
                    value_str = str(value).lower()
                elif isinstance(value, (int, float)):
                    value_str = str(value)
                else:
                    value_str = f'"{value}"'
                f.write(f'  - key: {key}\n')
                f.write(f'    value: {value_str}\n')

if __name__ == '__main__':
    generate_clang_tidy()

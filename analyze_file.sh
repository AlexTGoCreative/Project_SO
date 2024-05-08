#!/bin/bash

analyze_file() {
    file_path="$1"
    found_keyword=false
    # Check if file exists
    if [ ! -f "$file_path" ]; then
        echo "File '$file_path' not found."
        exit -1
    fi

    # Change file permissions to allow reading
    chmod +r "$file_path"
    
    # Count the number of lines, words, and characters
    num_lines=$(wc -l < "$file_path")
    num_words=$(wc -w < "$file_path")
    num_chars=$(wc -m < "$file_path")

    # Print analysis results
    echo "File '$file_path' analysis:"
    echo "Number of lines: $num_lines"
    echo "Number of words: $num_words"
    echo "Number of characters: $num_chars"
    
    # Check for keywords associated with corruption or malicious intent
    keywords=("corrupted" "dangerous" "risk" "attack" "malware" "malicious")
    for keyword in "${keywords[@]}"; do
        if grep -q -i "\<$keyword\>" "$file_path"; then
            echo "File '$file_path' contains the keyword '$keyword'"
            found_keyword=true
        fi
    done

    # Check for non-ASCII characters
    if grep -q '[^ -~]' "$file_path"; then
        echo "File '$file_path' contains non-ASCII characters"
	chmod -r "$file_path"
        exit 1
    fi

    if $found_keyword; then
	chmod -r "$file_path"
        exit 1 # in this way it prints all the dangerous keywords it encounters
    fi

    echo "File '$file_path' is safe"
    chmod -r "$file_path"
    exit 0
}

# Usage: ./analyze_file.sh <file_path>
if [ $# -ne 1 ]; then
    echo "Usage: $0 <file_path>"
    exit -1
fi

analyze_file "$1"

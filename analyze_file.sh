#!/bin/bash

analyze_file() {
    file_path="$1"
    file_name=$(basename "$file_path")
    
    # Check if file exists
    if [ ! -f "$file_path" ]; then
        echo "File '$file_path' not found."
        exit 1
    fi

    # Change file permissions to allow reading
    chmod +r "$file_path"
    
    # Count the number of lines, words, and characters
    num_lines=$(wc -l < "$file_path")
    num_words=$(wc -w < "$file_path")
    num_chars=$(wc -m < "$file_path")

    if [ "$num_lines" -lt 3 ] && [ "$num_words" -gt 1000 ] && [ "$num_chars" -gt 2000 ]; then
        echo "$file_name"
	chmod -r "$file_path"
	exit 0
    fi
    
    # Check for keywords associated with corruption or malicious intent
    keywords=("corrupted" "dangerous" "risk" "attack" "malware" "malicious")
    for keyword in "${keywords[@]}"; do
        if grep -q -i "\<$keyword\>" "$file_path"; then
            echo "$file_name"
	    chmod -r "$file_path"
	    exit 0
        fi
    done

    # Check for non-ASCII characters
    if grep -q '[^ -~]' "$file_path"; then
        echo "$file_name"
	chmod -r "$file_path"
	exit 0
    fi


    echo "SAFE"
    chmod -r "$file_path"
    exit 0
}

# Usage: ./analyze_file.sh <file_path>
if [ $# -ne 1 ]; then
    echo "Usage: $0 <file_path>"
    exit 1
fi

analyze_file "$1"
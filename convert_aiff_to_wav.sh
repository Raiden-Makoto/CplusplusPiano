#!/bin/bash

# Convert all AIFF files in src/NotesFF to WAV format
# Then delete the original AIFF files

NOTES_DIR="src/NotesFF"

if [ ! -d "$NOTES_DIR" ]; then
    echo "Error: Directory $NOTES_DIR not found"
    exit 1
fi

echo "Converting AIFF files to WAV format..."

converted=0
failed=0

for aiff_file in "$NOTES_DIR"/*.aiff; do
    if [ -f "$aiff_file" ]; then
        # Get the base name without extension
        base_name="${aiff_file%.aiff}"
        wav_file="${base_name}.wav"
        
        echo "Converting: $(basename "$aiff_file") -> $(basename "$wav_file")"
        
        # Convert using afconvert
        if afconvert -f WAVE -d LEI16 "$aiff_file" "$wav_file"; then
            # Delete the original AIFF file
            if rm "$aiff_file"; then
                echo "  ✓ Converted and deleted: $(basename "$aiff_file")"
                ((converted++))
            else
                echo "  ⚠ Converted but failed to delete: $(basename "$aiff_file")"
                ((converted++))
            fi
        else
            echo "  ✗ Failed to convert: $(basename "$aiff_file")"
            ((failed++))
        fi
    fi
done

echo ""
echo "Conversion complete!"
echo "  Converted: $converted files"
if [ $failed -gt 0 ]; then
    echo "  Failed: $failed files"
fi


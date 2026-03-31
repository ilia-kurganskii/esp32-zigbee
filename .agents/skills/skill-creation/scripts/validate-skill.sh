#!/bin/bash

# Skill validation script
echo "Validating skill structure..."

# Check for required files
if [ ! -f "SKILL.md" ]; then
    echo "ERROR: Missing SKILL.md file"
    exit 1
fi

# Check YAML frontmatter
if ! grep -q "^---" SKILL.md; then
    echo "ERROR: Missing YAML frontmatter"
    exit 1
fi

if ! grep -q "name:" SKILL.md; then
    echo "ERROR: Missing name in frontmatter"
    exit 1
fi

if ! grep -q "description:" SKILL.md; then
    echo "ERROR: Missing description in frontmatter"
    exit 1
fi

# Check for imperative instructions
if grep -q "could\|should\|would\|might" SKILL.md; then
    echo "WARNING: Found conditional language - consider using imperative form"
fi

# Check for examples
if ! grep -q "## Examples" SKILL.md; then
    echo "WARNING: No examples section found"
fi

# Check for validation section
if ! grep -q "## Validation\|## Testing" SKILL.md; then
    echo "WARNING: No validation section found"
fi

echo "Validation completed successfully!"

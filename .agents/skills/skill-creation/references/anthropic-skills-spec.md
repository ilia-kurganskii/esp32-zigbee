# Anthropic Skills Specification Reference

## Official Skill Structure
```
skill-name/
├── SKILL.md (required)
│   ├── YAML frontmatter (name, description required)
│   └── Markdown instructions
└── Bundled Resources (optional)
    ├── scripts/    - Executable code for deterministic tasks
    ├── references/ - Docs loaded into context as needed
    └── assets/      - Files used in output (templates, icons)
```

## Required YAML Frontmatter
```yaml
---
name: skill-name
description: Clear description of what this skill does and when to use it
---
```

## Key Principles
- Use imperative form ("Create this", not "You could create this")
- Be explicit and avoid vague instructions
- Provide step-by-step guidance
- Include validation mechanisms
- Define clear output formats
- Provide concrete examples

## Common Pitfalls to Avoid
- Vague instructions like "process appropriately"
- Missing validation steps
- Overfitting to specific examples
- Conditional language (could, should, would)
- Complex conditional structures

## Best Practices
- ✅ Clear, specific instructions
- ✅ Concrete examples with input/output
- ✅ Validation scripts or success criteria
- ✅ Explicit output format definitions
- ✅ Error handling guidance
- ✅ Test cases covering main use cases

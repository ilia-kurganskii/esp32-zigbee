# Agent Skills Directory

This directory contains reusable AI agent skills for development tasks. Each skill follows a standardized structure that can be used with any AI platform or agent system.

## Skill Structure

Each skill follows this universal structure:

```
.agents/skills/skill-name/
├── SKILL.md              # Main skill documentation and instructions
├── scripts/              # Executable validation and utility scripts
├── references/           # Reference documentation and guides
└── assets/               # Templates, examples, and reusable assets
```

## Available Skills

### 1. Skill Creation (`skill-creation/`)
**Purpose**: Guide for creating effective AI agent skills
- **Use Case**: When creating new skills or improving existing ones
- **Key Features**: Best practices, validation, templates
- **Validation Script**: `scripts/validate-skill.sh`

### 2. ESP32 Development (`espressif-sdk-development/`)
**Purpose**: Complete ESP32 development workflow using ESP-IDF
- **Use Case**: ESP32 firmware development, debugging, deployment
- **Key Features**: Environment setup, project creation, flashing
- **Validation Script**: `scripts/validate-espidf.sh`

## Using Skills

### For AI Agents
Load the `SKILL.md` file from any skill directory to provide the agent with:
- Step-by-step instructions
- Validation procedures
- Examples and templates
- Troubleshooting guides

### For Developers
1. **Reference**: Use `references/` folder for documentation
2. **Validation**: Run scripts in `scripts/` folder for verification
3. **Templates**: Use files in `assets/` as starting points

## Skill Standards

All skills follow these principles:
- ✅ Clear, imperative instructions
- ✅ Explicit validation steps
- ✅ Concrete examples
- ✅ Troubleshooting guidance
- ✅ Platform-agnostic content

## Adding New Skills

To add a new skill:
1. Create directory: `.agents/skills/your-skill-name/`
2. Add `SKILL.md` with proper frontmatter
3. Add optional `scripts/`, `references/`, `assets/` folders
4. Follow the established structure and patterns

## Compatibility

These skills are designed to work with:
- Any AI platform (Claude, GPT, etc.)
- Custom agent systems
- Manual development workflows
- CI/CD pipelines

The content focuses on universal development practices rather than platform-specific features.

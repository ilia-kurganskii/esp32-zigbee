---
name: skill-creation
description: Comprehensive guide for creating effective AI agent skills with proper structure, best practices, and validation
---

# Skill Creation Guide

This skill provides a systematic approach to creating high-quality AI agent skills following Anthropic's skill specification.

## Skill Structure

### Required Files
```
skill-name/
├── SKILL.md (required)
│   ├── YAML frontmatter (name, description required)
│   └── Markdown instructions
└── Bundled Resources (optional)
    ├── scripts/    - Executable code for deterministic tasks
    ├── references/  - Documentation loaded into context
    └── assets/      - Files used in output (templates, icons)
```

### YAML Frontmatter Template
```yaml
---
name: skill-name
description: Clear description of what this skill does and when to use it
---
```

## Writing Effective Instructions

### Core Principles
1. **Use imperative form** - Direct commands ("Create this", "Validate that")
2. **Be explicit** - Avoid vague instructions like "process appropriately"
3. **Provide step-by-step guidance** - Clear sequential instructions
4. **Include validation** - Scripts or checks to verify correctness
5. **Define output formats** - Use clear Markdown structures

### Instruction Patterns

#### Step-by-Step Process
```markdown
## Task Execution Steps

1. **Analyze requirements** - Review input and identify key components
2. **Create structure** - Set up necessary files and directories  
3. **Implement core logic** - Write main functionality
4. **Validate output** - Run validation script to verify correctness
5. **Finalize result** - Apply formatting and generate final output
```

#### Output Format Definition
```markdown
## Output Structure

# [Project Name]

## Executive Summary
[Brief overview of results]

## Key Findings
- Finding 1 with supporting details
- Finding 2 with supporting details

## Recommendations
- Recommendation 1 with rationale
- Recommendation 2 with rationale

## Next Steps
- Action item 1
- Action item 2
```

#### Examples Pattern
```markdown
## Examples

### Example 1: Basic Setup
**Input**: "Create a new React component with TypeScript"
**Output**: 
```typescript
interface Props {
  title: string;
  count: number;
}

export const MyComponent: React.FC<Props> = ({ title, count }) => {
  return (
    <div>
      <h1>{title}</h1>
      <p>Count: {count}</p>
    </div>
  );
};
```

### Example 2: Advanced Configuration
**Input**: "Configure Webpack for production build"
**Output**: [Detailed webpack configuration]
```

## Validation and Testing

### Create Validation Scripts
Place in `scripts/` directory for deterministic validation:

```bash
#!/bin/bash
# scripts/validate-skill.sh

echo "Validating skill output..."

# Check required files exist
if [ ! -f "output.txt" ]; then
    echo "ERROR: Missing output.txt"
    exit 1
fi

# Validate content format
if ! grep -q "## Executive Summary" output.txt; then
    echo "ERROR: Missing required section"
    exit 1
fi

echo "Validation passed!"
```

### Define Evaluation Tests
Create `evals/evals.json` for systematic testing:

```json
{
  "skill_name": "my-skill",
  "evals": [
    {
      "id": 1,
      "prompt": "Create a basic React component",
      "expected_output": "TypeScript React component with proper interface",
      "files": ["evals/files/sample-requirements.txt"],
      "expectations": [
        "Output includes TypeScript interface",
        "Component follows React best practices",
        "Validation script passes"
      ]
    }
  ]
}
```

## Best Practices

### Do's
- ✅ Use clear, specific instructions
- ✅ Provide concrete examples
- ✅ Include validation mechanisms
- ✅ Define explicit output formats
- ✅ Test with multiple scenarios
- ✅ Use imperative language
- ✅ Include error handling guidance

### Don'ts
- ❌ Use vague instructions ("handle appropriately")
- ❌ Assume context knowledge
- ❌ Skip validation steps
- ❌ Overfit to specific examples
- ❌ Use conditional language
- ❌ Ignore edge cases
- ❌ Skip documentation

## Skill Improvement Process

### Analysis Framework
When improving skills, analyze:
1. **Instruction clarity** - Are steps unambiguous?
2. **Example relevance** - Do examples cover key use cases?
3. **Validation coverage** - Does validation catch important errors?
4. **Output consistency** - Is format consistent across runs?
5. **Error handling** - How are edge cases handled?

### Iterative Improvement
1. **Test with diverse inputs** - Verify skill works beyond examples
2. **Review execution patterns** - Identify where agents struggle
3. **Add missing validation** - Catch common mistakes
4. **Clarify ambiguous instructions** - Replace vague language
5. **Expand examples** - Cover more use cases

## Common Pitfalls and Solutions

### Pitfall 1: Vague Instructions
**Problem**: "Process the document appropriately"
**Solution**: "Extract key sections, format as markdown, validate structure"

### Pitfall 2: Missing Validation
**Problem**: No way to verify correct output
**Solution**: Add validation script and success criteria

### Pitfall 3: Overfitting
**Problem**: Skill only works for specific examples
**Solution**: Generalize instructions to handle similar scenarios

### Pitfall 4: Complex Structure
**Problem**: Too many conditional branches
**Solution**: Simplify to linear process with clear steps

## Advanced Features

### Subagent Integration
```markdown
## Subagent Usage

When complex analysis is needed, use the code-reviewer subagent:

1. **Prepare context** - Gather relevant files and requirements
2. **Invoke subagent** - Use Agent tool with specific prompt
3. **Process results** - Format subagent output according to skill requirements
4. **Validate integration** - Ensure subagent output fits skill structure
```

### MCP Server Integration
```markdown
## External Tool Usage

Leverage available MCP servers for enhanced capabilities:

1. **Context7** - For technical documentation lookup
2. **Playwright** - For web automation and testing
3. **File operations** - For deterministic file handling

Always specify exact tool usage patterns and expected outputs.
```

## Quality Checklist

Before finalizing a skill, verify:

- [ ] Clear, descriptive name and description
- [ ] Step-by-step instructions in imperative form
- [ ] Explicit output format definition
- [ ] Relevant examples covering main use cases
- [ ] Validation script or success criteria
- [ ] Error handling guidance
- [ ] Test cases in evals.json
- [ ] Documentation in references/ if needed
- [ ] Assets/templates in assets/ if needed
- [ ] Instructions work for diverse inputs
- [ ] No vague or conditional language
- [ ] Proper YAML frontmatter structure

## Template for New Skills

Use this template as starting point:

```markdown
---
name: your-skill-name
description: Clear description of what this skill does and when to use it
---

# Skill Name

Brief overview of the skill's purpose and scope.

## When to Use This Skill
- Use case 1
- Use case 2
- Use case 3

## Prerequisites
- Requirement 1
- Requirement 2

## Execution Steps

1. **Step 1** - Clear action description
2. **Step 2** - Clear action description
3. **Step 3** - Clear action description

## Output Format

# [Result Title]

## Section 1
[Required content]

## Section 2
[Required content]

## Examples

### Example 1
**Input**: [Example input]
**Output**: [Example output]

## Validation
[How to verify correct execution]

## Common Issues and Solutions
- Issue 1: Solution
- Issue 2: Solution
```

This skill structure ensures consistency, reliability, and maintainability of AI agent capabilities.

# Instruction Router Logic

## Purpose
Динамічна селекція файлів інструкцій на основі класифікації запиту.

## Routing Matrix

### Core: ЗАВЖДИ завантажується (Для всіх запитів)
```
File: .clinerules
Tokens: 400
Purpose: Project context, core rules, key files
Fallback: YES (все ще можна працювати без решти)
```

---

### Conditional Routing by Classification

#### SIMPLE → Routing
```
Classification: SIMPLE
Confidence: > 0.85 (HIGH)

Load:
  ✓ .clinerules (400)

Skip:
  ✗ code-conventions.md
  ✗ architecture.md
  ✗ skeptical-architect.md
  ✗ anti-patterns.md
  ✗ production-ready.md

Total Tokens: 400
Time to Load: < 100ms
Execution: Immediate (no deliberation needed)

Example: Fix typo, rename, format, delete unused
```

#### CODE → Routing
```
Classification: CODE
Confidence: > 0.80 (HIGH)

Load:
  ✓ .clinerules (400)
  ✓ code-conventions.md (700)

Conditional:
  ? anti-patterns.md (only if "refactor" keyword)
  ? skeptical-architect.md (only if "critical" keyword)

Skip:
  ✗ architecture.md
  ✗ production-ready.md

Total Tokens: 1100 (base)
Total Tokens: 1500-1800 (if conditionals met)
Time to Load: < 200ms
Execution: Standard methodology (naming, validation, patterns)

Example: Add feature, implement function, modify logic
```

#### CODE_COMPLEX → Routing
```
Classification: CODE_COMPLEX
Confidence: > 0.75 (MEDIUM)

Load (MANDATORY):
  ✓ .clinerules (400)
  ✓ code-conventions.md (700)
  ✓ skeptical-architect.md (600)

Conditional:
  ? anti-patterns.md (if "safety" OR "security" keyword)
  ? architecture.md (if "component" OR "module" keyword)

Skip:
  ✗ production-ready.md (unless "config" keyword)

Total Tokens: 1700 (base)
Total Tokens: 2000-2400 (if conditionals met)
Time to Load: < 300ms
Execution: 7-step skeptical architect methodology

Example: Algorithm, edge cases, adaptive logic, critical features
```

#### ARCH → Routing
```
Classification: ARCH
Confidence: > 0.75 (MEDIUM)

Load (MANDATORY):
  ✓ .clinerules (400)
  ✓ architecture.md (600)
  ✓ skeptical-architect.md (600)

Conditional:
  ? code-conventions.md (if implementation phase)
  ? production-ready.md (if "transferability" keyword)

Skip:
  ✗ anti-patterns.md (unless specifically asked)

Total Tokens: 1600 (base)
Total Tokens: 2000-2600 (if conditionals met)
Time to Load: < 300ms
Execution: Architecture-first methodology (component design)

Example: New component, system design, integration
```

#### PROD → Routing
```
Classification: PROD
Confidence: > 0.78 (HIGH)

Load (MANDATORY):
  ✓ .clinerules (400)
  ✓ production-ready.md (900)
  ✓ skeptical-architect.md (600)

Conditional:
  ? code-conventions.md (if "code" OR "implement" keyword)
  ? anti-patterns.md (if "safety" OR "security" keyword)

Skip:
  ✗ architecture.md (unless "component" keyword)

Total Tokens: 1900 (base)
Total Tokens: 2300-2600 (if conditionals met)
Time to Load: < 300ms
Execution: Production checklist + methodology

Example: Release, OTA, config, backup, emergency
```

#### QUALITY → Routing
```
Classification: QUALITY
Confidence: > 0.80 (HIGH)

Load (MANDATORY):
  ✓ .clinerules (400)
  ✓ code-conventions.md (700)
  ✓ skeptical-architect.md (600)
  ✓ anti-patterns.md (700)

Conditional:
  ? architecture.md (if "design" OR "component" keyword)
  ? production-ready.md (if "release" OR "config" keyword)

Total Tokens: 2400 (base)
Total Tokens: 2600-3000 (if conditionals met)
Time to Load: < 400ms
Execution: Comprehensive review with all perspectives

Example: Code review, pattern analysis, security audit
```

---

## Context Integration

### pavlo_says.md Integration
**Для всіх класифікацій:**

```
After file selection:
1. Read pavlo_says.md
2. Search for related requirements (same topic)
3. Extract relevant entries
4. Add to context (max 200 tokens)
5. Include in total token budget

Formula:
  total_tokens = core_files_tokens + conditional_tokens + history_tokens
                = [400-900] + [0-600] + [0-200]
                = [400-1700 avg]
```

**Приклад:**
- User запитує про PID регулятор
- Система знаходить у pavlo_says.md попередні вимоги про PID
- Додає до контексту: "Користувач раніше просив: точність ±0.5°C, налаштування через web"
- Економить час на уточненнях

---

## Token Budget Allocation

### Per-Request Budget (Based on Classification)

| Type | Base | Conditional | History | Total |
|------|------|-------------|---------|-------|
| SIMPLE | 400 | 0-200 | 0-100 | 400-700 |
| CODE | 1100 | 0-400 | 0-100 | 1100-1600 |
| CODE_COMPLEX | 1700 | 0-600 | 0-100 | 1700-2400 |
| ARCH | 1600 | 0-400 | 0-100 | 1600-2100 |
| PROD | 1900 | 0-400 | 0-100 | 1900-2400 |
| QUALITY | 2400 | 0-200 | 0-100 | 2400-2700 |

**Average:** 850-1200 tokens/request (vs. 3000 before)
**Savings:** 65-72% 📊

---

## Routing Decision Tree

```
START: User Request
│
├─ Is it obvious typo/fix/delete?
│  └─ YES → SIMPLE (400) → GO
│  └─ NO → Continue
│
├─ Does it mention algorithm/critical/edge case?
│  └─ YES → CODE_COMPLEX (1700) → GO
│  └─ NO → Continue
│
├─ Does it mention architecture/component/module/design?
│  └─ YES → ARCH (1600) → GO
│  └─ NO → Continue
│
├─ Does it mention config/OTA/release/production?
│  └─ YES → PROD (1900) → GO
│  └─ NO → Continue
│
├─ Does it mention review/pattern/security/anti-pattern?
│  └─ YES → QUALITY (2400) → GO
│  └─ NO → Continue
│
├─ Does it mention add/implement/modify/function?
│  └─ YES → CODE (1100) → GO
│  └─ NO → Continue
│
└─ AMBIGUOUS (Confidence < 0.65)
   └─ Ask for clarification
```

---

## Conditional Routing Rules

### Rule 1: Complexity Escalation
```
IF classification = CODE
AND (keyword = "algorithm" OR keyword = "critical" OR keyword = "edge case")
THEN upgrade_to = CODE_COMPLEX
```

### Rule 2: Architecture Detection
```
IF (keyword IN ["component", "module", "interface", "flow"])
AND classification != ARCH
THEN add_file = architecture.md
```

### Rule 3: Safety Emphasis
```
IF (keyword IN ["safety", "security", "emergency"])
AND classification != QUALITY
THEN add_file = anti-patterns.md
```

### Rule 4: Production Context
```
IF (keyword IN ["production", "release", "deploy"])
AND classification != PROD
THEN swap_to = PROD
```

### Rule 5: History Relevance
```
IF pavlo_says.md contains recent entry for topic
THEN add_history_context = true
     history_tokens = min(200, relevant_size)
```

---

## File Load Order (for Context Building)

**Priority order (most important first):**
1. `.clinerules` (ALWAYS first - context)
2. Topic-specific (ARCH/CODE/PROD/etc.)
3. Skeptical-architect (if complex)
4. Anti-patterns (if safety/quality)
5. History from pavlo_says.md
6. Specific examples from codebase

---

## Performance Optimization

### Caching Strategy
```
Cache file contents (do not re-read):
├─ .clinerules (400) - cache forever
├─ architecture.md (600) - cache 1 week
├─ code-conventions.md (700) - cache 1 week
├─ production-ready.md (900) - cache 2 weeks
├─ skeptical-architect.md (600) - cache forever
├─ anti-patterns.md (700) - cache 2 weeks
└─ pavlo_says.md - read fresh (dynamic)

Benefits:
- Faster file loading (< 50ms vs 200ms read)
- Reduced disk I/O
- More deterministic response time
```

### Parallel Loading (if possible)
```
Task 1: Load .clinerules
Task 2: Load primary files (ARCH/CODE/PROD/etc.) in parallel
Task 3: Load conditionals
Task 4: Extract history from pavlo_says.md
Task 5: Merge and deduplicate

Total: < 300ms vs sequential 800ms
```

---

## Deduplication Rules

**When merging multiple files:**

```
IF section_appears_in (file1, file2):
  - Keep most specific version
  - Remove duplicate from less specific
  - Example: validation pattern in both files
    → Keep from code-conventions.md (most specific)
    → Remove from skeptical-architect.md (general reference)
```

---

## Fallback Routing

**If classification fails (confidence < 0.65):**

```
1. Default to CODE classification
   - Safe middle ground
   - Includes naming conventions + core rules
   - Allows execution while clarifying

2. Ask user for context:
   "Це звучить як [GUESS]. Правильно?
    - Додати новий код? (CODE)
    - Перевірити архітектуру? (ARCH)
    - Підготувати релізу? (PROD)
    - Щось інше?"

3. Re-route based on response
   - Update classification history
   - Learn for future similar requests
```

---

## Examples: Routing in Action

### Example 1: Simple Typo
```
Request: "Fix typo in sensor.cpp line 42: 'temperaturee' → 'temperature'"

Classification Engine:
  - Keywords: ["fix", "typo"]
  - Confidence: 0.96 (HIGH)
  - Type: SIMPLE

Routing:
  Core: .clinerules (400)
  Conditionals: None
  History: None
  Total: 400 tokens

Load time: < 100ms
Response: Immediate fix, no deliberation

Action: Change code, respond
```

### Example 2: Add Feature
```
Request: "Додай LED індикатор, який горить червоно при
          перегріві (>25°C) і зелено при нормі"

Classification Engine:
  - Keywords: ["add", "indicator", "temperature", "LED"]
  - Scope: Single component
  - Confidence: 0.88 (HIGH)
  - Type: CODE

Routing:
  Core: .clinerules (400)
  Primary: code-conventions.md (700)
  Conditionals: None (no "critical" keyword)
  History: None
  Total: 1100 tokens

Load time: < 200ms
Response: Create led_indicator component following conventions

Action: New file, follow patterns, integrate
```

### Example 3: Complex Algorithm
```
Request: "Реалізуй адаптивне навчання для PID. Система
          повинна вчитися на історії температури й авто-настройватися"

Classification Engine:
  - Keywords: ["adaptive", "learning", "algorithm", "history"]
  - Complexity: HIGH (multi-component, new algorithm)
  - Confidence: 0.92 (HIGH)
  - Type: CODE_COMPLEX

Routing:
  Core: .clinerules (400)
  Primary: code-conventions.md (700)
  Secondary: skeptical-architect.md (600)
  Conditionals: None
  History: Check pavlo_says.md for PID requirements
  Total: 1700 + 150 = 1850 tokens

Load time: < 300ms
Response: 7-step methodology (authoritative → critic → alternatives → synthesis)

Action: Comprehensive solution with trade-offs
```

### Example 4: Production Release
```
Request: "Підготуй систему до релізу v5.0. Потрібна OTA,
          backup конфігурації, factory reset, update history"

Classification Engine:
  - Keywords: ["release", "OTA", "backup", "production"]
  - Production-critical: YES
  - Confidence: 0.91 (HIGH)
  - Type: PROD

Routing:
  Core: .clinerules (400)
  Primary: production-ready.md (900)
  Secondary: skeptical-architect.md (600)
  Conditionals: code-conventions.md (700) - implementation phase
  History: Recent OTA notes from pavlo_says.md
  Total: 2100 + 100 = 2200 tokens

Load time: < 350ms
Response: Complete production checklist + implementation

Action: Release preparation steps
```

### Example 5: Code Review
```
Request: "Перевір цей PID код на anti-patterns,
          безпеку і ефективність"

Classification Engine:
  - Keywords: ["review", "anti-pattern", "security", "efficiency"]
  - Intent: EVALUATION
  - Confidence: 0.87 (HIGH)
  - Type: QUALITY

Routing:
  Core: .clinerules (400)
  Primary: code-conventions.md (700)
           skeptical-architect.md (600)
           anti-patterns.md (700)
  Conditionals: None
  History: Previous PID requirements
  Total: 2400 + 100 = 2500 tokens

Load time: < 400ms
Response: Comprehensive review with patterns + improvements

Action: Analysis + suggestions
```

---

## Monitoring & Feedback

### Metrics to Track
```
Per Request:
├─ Classification type
├─ Confidence score
├─ Files loaded
├─ Total tokens used
├─ Execution time
├─ User satisfaction (implicit from follow-ups)

Per Day:
├─ Average tokens/request
├─ Distribution: SIMPLE/CODE/CODE_COMPLEX/etc.
├─ Misclassifications (when user corrects)
├─ Fallback usage (ambiguous requests)

Per Week:
├─ Token savings vs baseline (3000)
├─ Classification accuracy
├─ New patterns discovered
├─ Rules refinement needed
```

### Adjustment Protocol
```
IF misclassification_rate > 10%:
  → Review and update CLASSIFIER_RULES.md
  → Recalibrate confidence thresholds
  → Add new keyword indicators

IF average_tokens > 1000:
  → Reduce conditional file loading
  → Optimize token allocation
  → Consider file consolidation

IF user_satisfaction ↓:
  → Add missing context
  → Improve fallback UX
  → Clarify routing decisions
```

---

## Integration with Claude

**Instructions for Claude (implicit activation):**

```markdown
# AUTOMATIC ROUTING SYSTEM

On every user request:

1. CLASSIFY using CLASSIFIER_RULES.md
   ├─ Extract keywords
   ├─ Assess scope & complexity
   ├─ Calculate confidence
   └─ Determine classification type

2. ROUTE using ROUTER_LOGIC.md
   ├─ Select core files
   ├─ Apply conditionals
   ├─ Load history context
   └─ Total token budget

3. LOAD context silently
   ├─ Merge files (no duplication)
   ├─ Prepare pavlo_says.md history
   └─ Ready for execution

4. EXECUTE without mentioning process
   ├─ Use all loaded rules
   ├─ Follow selected methodology
   └─ Respond with solution only

5. RESPONSE
   └─ Solution applies all rules automatically
      (quality proves the methodology works)
```

---

## Status

✅ Routing matrix defined
✅ Token allocation optimized
✅ Conditional rules specified
✅ Examples provided
🔄 Ready for integration with Claude instructions

**Use this for Claude custom tools**
```json
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "type": "object",
  "properties": {
    "prompt": { "type": "string", "minLength": 1 }
  },
  "required": ["prompt"],
  "additionalProperties": false
}
```

**Common mistakes**
- Wrong `$schema` version; using `nullable`; `required` keys not in `properties`; `default` values that don’t validate; `pattern` without `type: string`.
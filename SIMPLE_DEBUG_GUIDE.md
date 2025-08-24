# Simple Debug Guide - Find App Freeze

## Quick Start (5 minutes)

### Step 1: Set ONE Simple Breakpoint
1. Open `src/ui/src/main_window.cpp`
2. Find line ~1283 with `add_media_to_timeline`
3. Click in the left margin to add a red dot (breakpoint)

### Step 2: Start Debugging
1. Press `F5` (or Debug → Start Debugging)
2. Choose "Debug Video Editor (Qt Debug)"
3. Wait for app to start

### Step 3: Reproduce the Freeze
1. Try to add media that causes the freeze
2. When it hits the breakpoint, press `F10` to step through
3. Watch which line takes too long

## What to Look For

### Signs of the Problem:
- ⏰ **Pressing F10 and waiting forever** = Found the freeze line!
- 🔄 **Same line repeating many times** = Infinite loop
- 📊 **Large numbers in variables** = Processing too much data

### Common Freeze Locations:
1. **Timeline processing** (flushTimelineBatch function)
2. **Paint events** (when drawing the timeline)
3. **File loading** (when opening large videos)

## Emergency Stop
- Press `Shift + F5` to stop debugging anytime
- Or close VS Code if stuck

## Quick Variables to Check
When stopped at breakpoint, hover mouse over:
- `batch.size()` - Should be small (< 100)
- `timeline_items.size()` - Should be reasonable
- Any variable with "count" or "size" in the name

## Next Level (If Step 1 Works)
1. Add breakpoint in `flushTimelineBatch` function (~line 1473)
2. Add breakpoint in `paintEvent` function in timeline_panel.cpp
3. Use same F5 → F10 stepping process

## Success = Finding the Slow Line
When F10 takes 5+ seconds on one line, you found it! 
Take a screenshot and note the line number.

---
**Remember: You're looking for the ONE line that takes forever when you press F10**

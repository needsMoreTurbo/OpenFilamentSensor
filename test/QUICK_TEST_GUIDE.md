# Quick Test Guide

## 🚀 Run Everything

```bash
cd test
./build_and_run_all_tests.sh
```

Expected output: All tests pass with green ✓ indicators.

## 📋 Test Suite Breakdown

### C++ Tests (4 files)
1. **pulse_simulator** - FilamentMotionSensor comprehensive tests (existing)
2. **test_jam_detector** - JamDetector unit tests (12 cases)
3. **test_sdcp_protocol** - SDCPProtocol unit tests (5 cases)
4. **test_additional_edge_cases** - Edge cases & integration (9 cases) **NEW**

### Python Tests (1 file)
- **test_tools.py** - Build tools validation (13 test classes, 42+ methods)

### JavaScript Tests (1 file)
- **test_distributor.js** - Web components validation (20 tests)

## 🔧 Requirements

**For C++ tests:**
- g++ with C++11 support

**For Python tests:**
- Python 3.6+
- No external dependencies (PyYAML optional)

**For JavaScript tests:**
- Node.js 12+
- No external dependencies

## 📊 Expected Results
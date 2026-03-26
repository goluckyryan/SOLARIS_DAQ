#include <cstdlib>
#include <string>
#include <vector>
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <cmath>

#include "../ClassDigitizer2Gen.h"

//^####################################################
// Test: Read, Write, Save, Load ALL registers
//
// Usage: ./test [url]
//   default url: dig2://192.168.0.254/
//####################################################

// Generate a test value different from the current value for a given register
// Uses ~25% of range to avoid boundary issues (VX2730 divides some params by 4)
static std::string MakeTestValue(const Reg &reg, const std::string &currentVal) {
  auto answers = reg.GetAnswers();
  ANSTYPE atype = reg.GetAnswerType();

  if (atype == ANSTYPE::INTEGER || atype == ANSTYPE::FLOAT) {
    if (answers.size() >= 3) {
      double minVal  = atof(answers[0].first.c_str());
      double maxVal  = atof(answers[1].first.c_str());
      double step    = atof(answers[2].first.c_str());
      double curVal  = atof(currentVal.c_str());

      if (step < 1) step = 1; // handle step=0 (e.g. CFDFraction)

      // cap maxVal to avoid int overflow
      if (maxVal > 2e9) maxVal = 2e9;

      // use ~25% of range, rounded to step
      double range = maxVal - minVal;
      double testVal = minVal + std::round(range * 0.25 / step) * step;
      if (testVal > maxVal) testVal = minVal + step;
      if (testVal < minVal) testVal = minVal;
      // if same as current, shift by one step
      if (std::abs(testVal - curVal) < 0.001) {
        testVal += step;
        if (testVal > maxVal) testVal = minVal + step;
      }

      if (atype == ANSTYPE::INTEGER) {
        return std::to_string((long long)testVal);
      } else {
        return std::to_string(testVal);
      }
    }
  } else if (atype == ANSTYPE::COMBOX || atype == ANSTYPE::NONE) {
    if (answers.size() >= 2) {
      // pick a different answer from current
      for (size_t i = 0; i < answers.size(); i++) {
        if (answers[i].first != currentVal) {
          return answers[i].first;
        }
      }
      return answers[0].first; // fallback
    }
  }
  return ""; // can't generate a test value
}

static bool TestReadAllSettings(Digitizer2Gen *digi) {
  printf("============================================\n");
  printf("  Test 1: ReadAllSettings\n");
  printf("============================================\n");

  digi->ReadAllSettings();

  int nCh = digi->GetNChannels();
  std::string fpga = digi->GetFPGAType();
  std::string model = digi->GetModelName();
  printf("  Model: %s, FPGA: %s, Channels: %d\n", model.c_str(), fpga.c_str(), nCh);

  bool ok = true;

  std::string sn = digi->GetSettingValueFromMemory(PHA::DIG::SerialNumber);
  printf("  SerialNumber from memory: %s\n", sn.c_str());
  if (sn.empty()) { printf("  FAIL: SerialNumber is empty\n"); ok = false; }

  if (fpga == DPPType::PHA) {
    std::string rl = digi->GetSettingValueFromMemory(PHA::CH::RecordLength, 0);
    printf("  PHA CH0 RecordLength: %s\n", rl.c_str());
    if (rl.empty()) { printf("  FAIL: RecordLength ch0 is empty\n"); ok = false; }
  }
  if (fpga == DPPType::PSD) {
    std::string rl = digi->GetSettingValueFromMemory(PSD::CH::RecordLength, 0);
    printf("  PSD CH0 RecordLength: %s\n", rl.c_str());
    if (rl.empty()) { printf("  FAIL: RecordLength ch0 is empty\n"); ok = false; }
  }

  printf("  Test 1 %s\n\n", ok ? "PASSED" : "FAILED");
  return ok;
}

// Helper: read a register value from the digitizer
static std::string ReadReg(Digitizer2Gen *digi, const Reg &reg, int ch) {
  if (reg.GetType() == TYPE::DIG) return digi->ReadValue(reg);
  return digi->ReadValue(reg, ch);
}

// Helper: write a register value to the digitizer
static bool WriteReg(Digitizer2Gen *digi, const Reg &reg, const std::string &val, int ch) {
  if (reg.GetType() == TYPE::DIG) return digi->WriteValue(reg, val);
  return digi->WriteValue(reg, val, ch);
}

// Helper: read from memory
static std::string ReadMemReg(Digitizer2Gen *digi, const Reg &reg, int ch) {
  if (reg.GetType() == TYPE::DIG) return digi->GetSettingValueFromMemory(reg);
  return digi->GetSettingValueFromMemory(reg, ch);
}

// Generate ALL test values for a register
static std::vector<std::string> MakeAllTestValues(const Reg &reg) {
  std::vector<std::string> vals;
  auto answers = reg.GetAnswers();
  ANSTYPE atype = reg.GetAnswerType();

  if (atype == ANSTYPE::INTEGER || atype == ANSTYPE::FLOAT) {
    if (answers.size() >= 3) {
      double minVal = atof(answers[0].first.c_str());
      double maxVal = atof(answers[1].first.c_str());
      double step   = atof(answers[2].first.c_str());
      if (step < 1) step = 1;
      if (maxVal > 2e9) maxVal = 2e9;

      // 5 spread values: 10%, 25%, 50%, 75%, 90% of range
      double percents[] = {0.10, 0.25, 0.50, 0.75, 0.90};
      double range = maxVal - minVal;
      for (double p : percents) {
        double tv = minVal + std::round(range * p / step) * step;
        if (tv < minVal) tv = minVal;
        if (tv > maxVal) tv = maxVal;
        // avoid duplicates
        std::string s = (atype == ANSTYPE::INTEGER) ? std::to_string((long long)tv) : std::to_string(tv);
        bool dup = false;
        for (auto &v : vals) if (v == s) { dup = true; break; }
        if (!dup) vals.push_back(s);
      }
    }
  } else if (atype == ANSTYPE::COMBOX || atype == ANSTYPE::NONE) {
    // all valid enum values
    for (size_t i = 0; i < answers.size(); i++) {
      if (!answers[i].first.empty()) {
        vals.push_back(answers[i].first);
      }
    }
  }
  return vals;
}

// Test write/readback for a single register with MULTIPLE values
// Returns: number of values tested, fills passCount/failCount
static int TestOneRegister(Digitizer2Gen *digi, const Reg &reg, int ch, const std::string &model,
                           int &passCount, int &failCount) {

  // skip model-specific exclusions
  if (model != "VX2730" && reg.GetPara() == PSD::CH::ChGain.GetPara()) return -1;

  // read current value
  std::string saved = ReadReg(digi, reg, ch);
  if (saved.empty()) return -1;

  // generate all test values
  std::vector<std::string> testVals = MakeAllTestValues(reg);
  if (testVals.empty()) return -1;

  ANSTYPE atype = reg.GetAnswerType();
  auto answers = reg.GetAnswers();
  int step = 1;
  if ((atype == ANSTYPE::INTEGER || atype == ANSTYPE::FLOAT) && answers.size() >= 3) {
    step = atoi(answers[2].first.c_str());
    if (step < 1) step = 1;
  }

  int tested = 0;
  for (const auto &testVal : testVals) {
    // write
    bool wOk = WriteReg(digi, reg, testVal, ch);
    if (!wOk) {
      if (reg.GetType() == TYPE::CH)
        printf("    -> ch%02d %-30s: WRITE REJECTED val=%s\n", ch, reg.GetPara().c_str(), testVal.c_str());
      else
        printf("    -> %-30s: WRITE REJECTED val=%s\n", reg.GetPara().c_str(), testVal.c_str());
      failCount++;
      tested++;
      continue;
    }

    // read back
    std::string readback = ReadReg(digi, reg, ch);
    std::string memVal = ReadMemReg(digi, reg, ch);

    // compare
    bool match = false;
    if (atype == ANSTYPE::INTEGER || atype == ANSTYPE::FLOAT) {
      match = (abs(atoi(readback.c_str()) - atoi(testVal.c_str())) <= 2 * step)
           && (readback == memVal);
    } else {
      std::string rbL = readback, tvL = testVal, mmL = memVal;
      for (auto &c : rbL) c = tolower(c);
      for (auto &c : tvL) c = tolower(c);
      for (auto &c : mmL) c = tolower(c);
      match = (rbL == tvL) && (mmL == rbL);
    }

    if (match) {
      passCount++;
    } else {
      failCount++;
      if (reg.GetType() == TYPE::CH)
        printf("    -> ch%02d %-30s: wrote=%s readback=%s mem=%s\n",
               ch, reg.GetPara().c_str(), testVal.c_str(), readback.c_str(), memVal.c_str());
      else
        printf("    -> %-30s: wrote=%s readback=%s mem=%s\n",
               reg.GetPara().c_str(), testVal.c_str(), readback.c_str(), memVal.c_str());
    }
    tested++;
  }

  // restore original value
  WriteReg(digi, reg, saved, ch);
  return tested;
}

static bool TestAllChannelRegisters(Digitizer2Gen *digi) {
  printf("============================================\n");
  printf("  Test 2: Write/Readback ALL channel RW registers (multi-value)\n");
  printf("============================================\n");

  int nCh = digi->GetNChannels();
  std::string fpga = digi->GetFPGAType();
  std::string model = digi->GetModelName();
  bool ok = true;
  int passCount = 0, failCount = 0, skipCount = 0;

  // Use runtime-adjusted settings from digitizer (not static definitions)
  const std::vector<Reg> &chAllSettings = digi->GetChSettings(0);

  // test on ch 0 and last channel
  int testChannels[] = {0, nCh - 1};

  for (int chIdx = 0; chIdx < 2; chIdx++) {
    int ch = testChannels[chIdx];
    printf("  --- Channel %d ---\n", ch);

    for (size_t i = 0; i < chAllSettings.size(); i++) {
      const Reg &reg = chAllSettings[i];

      if (reg.ReadWrite() != RW::ReadWrite) {
        skipCount++;
        continue;
      }

      int pBefore = passCount, fBefore = failCount;
      int result = TestOneRegister(digi, reg, ch, model, passCount, failCount);
      if (result <= 0) {
        skipCount++;
      } else if (failCount > fBefore) {
        ok = false;
        printf("  FAIL ch%02d [%2zu] %-35s (%d/%d passed)\n", ch, i, reg.GetPara().c_str(),
               passCount - pBefore, result);
      }
    }
  }

  printf("  Channel registers: %d passed, %d failed, %d skipped\n", passCount, failCount, skipCount);
  printf("  Test 2 %s\n\n", ok ? "PASSED" : "FAILED");
  return ok;
}

static bool TestAllBoardRegisters(Digitizer2Gen *digi) {
  printf("============================================\n");
  printf("  Test 3: Write/Readback ALL board RW registers\n");
  printf("============================================\n");

  std::string fpga = digi->GetFPGAType();
  std::string model = digi->GetModelName();
  bool ok = true;
  int passCount = 0, failCount = 0, skipCount = 0;

  // Use runtime-adjusted settings from digitizer
  const std::vector<Reg> &bdAllSettings = digi->GetBoardSettings();

  for (size_t i = 0; i < bdAllSettings.size(); i++) {
    const Reg &reg = bdAllSettings[i];

    if (reg.ReadWrite() != RW::ReadWrite) {
      skipCount++;
      continue;
    }

    // skip model-specific
    if (model == "VX2740" && reg.GetPara() != PHA::DIG::TempSensADC0.GetPara()) {
      // VX2740 only has TempSensADC0 readable among ReadOnly, but RW should be fine
    }

    int pBefore = passCount, fBefore = failCount;
    int result = TestOneRegister(digi, reg, -1, model, passCount, failCount);
    if (result <= 0) {
      skipCount++;
    } else if (failCount > fBefore) {
      ok = false;
      printf("  FAIL [%2zu] %-35s (%d/%d passed)\n", i, reg.GetPara().c_str(),
             passCount - pBefore, result);
    }
  }

  printf("  Board registers: %d passed, %d failed, %d skipped\n", passCount, failCount, skipCount);
  printf("  Test 3 %s\n\n", ok ? "PASSED" : "FAILED");
  return ok;
}

static bool TestLVDSRegisters(Digitizer2Gen *digi) {
  printf("============================================\n");
  printf("  Test 4: Write/Readback LVDS registers\n");
  printf("============================================\n");

  std::string fpga = digi->GetFPGAType();
  std::string model = digi->GetModelName();
  bool ok = true;
  int passCount = 0, failCount = 0, skipCount = 0;

  const std::vector<Reg> &lvdsSettings = (fpga == DPPType::PHA) ? PHA::LVDS::AllSettings : PSD::LVDS::AllSettings;

  for (int grp = 0; grp < 4; grp++) {
    for (size_t i = 0; i < lvdsSettings.size(); i++) {
      const Reg &reg = lvdsSettings[i];
      if (reg.ReadWrite() != RW::ReadWrite) { skipCount++; continue; }

      int pBefore = passCount, fBefore = failCount;
      int result = TestOneRegister(digi, reg, grp, model, passCount, failCount);
      if (result <= 0) {
        skipCount++;
      } else if (failCount > fBefore) {
        ok = false;
        printf("  FAIL LVDS[%d] %-35s (%d/%d passed)\n", grp, reg.GetPara().c_str(),
               passCount - pBefore, result);
      }
    }
  }

  printf("  LVDS registers: %d passed, %d failed, %d skipped\n", passCount, failCount, skipCount);
  printf("  Test 4 %s\n\n", ok ? "PASSED" : "FAILED");
  return ok;
}

static bool TestWriteAllChannels(Digitizer2Gen *digi) {
  printf("============================================\n");
  printf("  Test 5: Write ALL channels (ch=-1) for all integer RW regs\n");
  printf("============================================\n");

  int nCh = digi->GetNChannels();
  std::string fpga = digi->GetFPGAType();
  std::string model = digi->GetModelName();
  bool ok = true;
  int passCount = 0, failCount = 0, skipCount = 0;

  // Use runtime-adjusted settings from digitizer
  const std::vector<Reg> &chAllSettings = digi->GetChSettings(0);

  for (size_t i = 0; i < chAllSettings.size(); i++) {
    const Reg &reg = chAllSettings[i];

    if (reg.ReadWrite() != RW::ReadWrite) { skipCount++; continue; }
    if (reg.GetAnswerType() != ANSTYPE::INTEGER) { skipCount++; continue; }
    if (model != "VX2730" && reg.GetPara() == PSD::CH::ChGain.GetPara()) { skipCount++; continue; }

    auto answers = reg.GetAnswers();
    if (answers.size() < 3) { skipCount++; continue; }

    double minVal = atof(answers[0].first.c_str());
    double maxVal = atof(answers[1].first.c_str());
    double step   = atof(answers[2].first.c_str());
    if (step < 1) step = 1;
    if (maxVal > 2e9) maxVal = 2e9;
    double range  = maxVal - minVal;
    double tv     = minVal + std::round(range * 0.25 / step) * step;
    if (tv > maxVal) tv = minVal + step;
    std::string testVal = std::to_string((long long)tv);

    // save current values
    std::vector<std::string> saved(nCh);
    for (int ch = 0; ch < nCh; ch++) {
      saved[ch] = digi->ReadValue(reg, ch);
    }

    // write to all channels
    bool wOk = digi->WriteValue(reg, testVal, -1);
    if (!wOk) {
      printf("  FAIL %-35s write-all returned false\n", reg.GetPara().c_str());
      failCount++;
      ok = false;
      // restore
      for (int ch = 0; ch < nCh; ch++) digi->WriteValue(reg, saved[ch], ch);
      continue;
    }

    // read back each channel
    // tolerance: allow digitizer rounding (VX2730 may round to 8ns tick boundary)
    int chOk = 0;
    int expected = atoi(testVal.c_str());
    int tolerance = std::max(8, (int)step);

    for (int ch = 0; ch < nCh; ch++) {
      std::string rb = digi->ReadValue(reg, ch);
      int diff = abs(atoi(rb.c_str()) - expected);
      if (diff <= tolerance) {
        chOk++;
      } else {
        if (chOk == ch) { // only print first few failures
          printf("  FAIL %-35s ch%02d: wrote=%s got=%s (diff=%d tol=%d)\n",
                 reg.GetPara().c_str(), ch, testVal.c_str(), rb.c_str(), diff, tolerance);
        }
      }
    }

    if (chOk == nCh) {
      passCount++;
    } else {
      printf("  FAIL %-35s %d/%d channels matched\n", reg.GetPara().c_str(), chOk, nCh);
      failCount++;
      ok = false;
    }

    // restore
    for (int ch = 0; ch < nCh; ch++) digi->WriteValue(reg, saved[ch], ch);
  }

  printf("  Write-all: %d passed, %d failed, %d skipped\n", passCount, failCount, skipCount);
  printf("  Test 5 %s\n\n", ok ? "PASSED" : "FAILED");
  return ok;
}

static bool TestSaveAndLoad(Digitizer2Gen *digi) {
  printf("============================================\n");
  printf("  Test 6: Save and Load settings file\n");
  printf("============================================\n");

  bool ok = true;
  int nCh = digi->GetNChannels();
  std::string fpga = digi->GetFPGAType();

  printf("  Step 1: ReadAllSettings from digitizer\n");
  digi->ReadAllSettings();

  const char * saveFile = "/tmp/test_settings_save.txt";
  printf("  Step 2: Save settings to %s\n", saveFile);
  int saveRet = digi->SaveSettingsToFile(saveFile);
  if (saveRet != 1) {
    printf("  FAIL: SaveSettingsToFile returned %d\n", saveRet);
    ok = false;
  } else {
    printf("  Save OK\n");
  }

  // store current values for ALL RW ch registers
  const std::vector<Reg> &chAll = (fpga == DPPType::PHA) ? PHA::CH::AllSettings : PSD::CH::AllSettings;

  struct SavedVal { std::string val; };
  // save [regIndex][ch]
  std::vector<std::vector<SavedVal>> origVals(chAll.size(), std::vector<SavedVal>(nCh));
  for (size_t i = 0; i < chAll.size(); i++) {
    if (chAll[i].ReadWrite() != RW::ReadWrite) continue;
    for (int ch = 0; ch < nCh; ch++) {
      origVals[i][ch].val = digi->GetSettingValueFromMemory(chAll[i], ch);
    }
  }

  // modify a few settings
  Reg rlReg = (fpga == DPPType::PHA) ? PHA::CH::RecordLength : PSD::CH::RecordLength;
  Reg thReg = (fpga == DPPType::PHA) ? PHA::CH::TriggerThreshold : PSD::CH::TriggerThreshold;
  printf("  Step 3: Modify ch0 RecordLength=2048, Threshold=999\n");
  digi->WriteValue(rlReg, "2048", 0);
  digi->WriteValue(thReg, "999", 0);

  std::string modRL = digi->ReadValue(rlReg, 0);
  std::string modTH = digi->ReadValue(thReg, 0);
  printf("  After modify: ch0 RecordLength=%s, Threshold=%s\n", modRL.c_str(), modTH.c_str());

  // load back
  printf("  Step 4: Load settings from %s\n", saveFile);
  bool loadOk = digi->LoadSettingsFromFile(saveFile);
  if (!loadOk) {
    printf("  FAIL: LoadSettingsFromFile returned false\n");
    ok = false;
  }

  // read back and verify ALL RW registers
  printf("  Step 5: ReadAllSettings and verify ALL channel RW registers\n");
  digi->ReadAllSettings();

  int matchCount = 0, mismatchCount = 0;
  for (size_t i = 0; i < chAll.size(); i++) {
    if (chAll[i].ReadWrite() != RW::ReadWrite) continue;
    for (int ch = 0; ch < nCh; ch++) {
      std::string cur = digi->GetSettingValueFromMemory(chAll[i], ch);
      std::string orig = origVals[i][ch].val;

      // compare: for integer/float, compare numerically; for combox, compare strings
      bool isMatch;
      if (chAll[i].GetAnswerType() == ANSTYPE::INTEGER) {
        isMatch = (atoi(cur.c_str()) == atoi(orig.c_str()));
      } else if (chAll[i].GetAnswerType() == ANSTYPE::FLOAT) {
        isMatch = (std::abs(atof(cur.c_str()) - atof(orig.c_str())) < 0.01);
      } else {
        isMatch = (cur == orig);
      }

      if (isMatch) {
        matchCount++;
      } else {
        mismatchCount++;
        printf("  MISMATCH ch%02d %-30s: orig=%s now=%s\n",
               ch, chAll[i].GetPara().c_str(), orig.c_str(), cur.c_str());
      }
    }
  }

  printf("  Verification: %d matched, %d mismatched\n", matchCount, mismatchCount);
  if (mismatchCount > 0) ok = false;

  remove(saveFile);
  printf("  Test 6 %s\n\n", ok ? "PASSED" : "FAILED");
  return ok;
}

static bool TestSaveLoadRoundTrip(Digitizer2Gen *digi) {
  printf("============================================\n");
  printf("  Test 7: Full save/load round-trip (file compare)\n");
  printf("============================================\n");

  bool ok = true;
  const char * file1 = "/tmp/test_roundtrip_1.txt";
  const char * file2 = "/tmp/test_roundtrip_2.txt";

  printf("  Step 1: ReadAndSave to %s\n", file1);
  int ret1 = digi->ReadAndSaveSettingsToFile(file1);
  if (ret1 != 1) { printf("  FAIL: first save returned %d\n", ret1); ok = false; }

  printf("  Step 2: Load from %s\n", file1);
  bool loadOk = digi->LoadSettingsFromFile(file1);
  if (!loadOk) { printf("  FAIL: load returned false\n"); ok = false; }

  printf("  Step 3: ReadAndSave to %s\n", file2);
  int ret2 = digi->ReadAndSaveSettingsToFile(file2);
  if (ret2 != 1) { printf("  FAIL: second save returned %d\n", ret2); ok = false; }

  printf("  Step 4: Compare RW entries between the two files\n");

  FILE * f1 = fopen(file1, "r");
  FILE * f2 = fopen(file2, "r");
  if (!f1 || !f2) {
    printf("  FAIL: cannot open files for comparison\n");
    if (f1) fclose(f1);
    if (f2) fclose(f2);
    ok = false;
  } else {
    char line1[512], line2[512];
    int lineNum = 0, rwMatch = 0, rwMismatch = 0;

    while (fgets(line1, sizeof(line1), f1) && fgets(line2, sizeof(line2), f2)) {
      lineNum++;
      char l1copy[512], l2copy[512];
      char *saveptr1 = NULL, *saveptr2 = NULL;
      strncpy(l1copy, line1, sizeof(l1copy));
      strncpy(l2copy, line2, sizeof(l2copy));

      strtok_r(l1copy, "!", &saveptr1);
      char* tok1_rw = strtok_r(NULL, "!", &saveptr1);
      strtok_r(l2copy, "!", &saveptr2);
      char* tok2_rw = strtok_r(NULL, "!", &saveptr2);

      if (tok1_rw && atoi(tok1_rw) == 2 && tok2_rw && atoi(tok2_rw) == 2) {
        if (strcmp(line1, line2) == 0) {
          rwMatch++;
        } else {
          line1[strcspn(line1, "\n")] = 0;
          line2[strcspn(line2, "\n")] = 0;
          printf("  DIFF line %d:\n    file1: %s\n    file2: %s\n", lineNum, line1, line2);
          rwMismatch++;
        }
      }
    }
    fclose(f1);
    fclose(f2);
    printf("  RW entries: %d matched, %d mismatched\n", rwMatch, rwMismatch);
    if (rwMismatch > 0) ok = false;
  }

  remove(file1);
  remove(file2);
  printf("  Test 7 %s\n\n", ok ? "PASSED" : "FAILED");
  return ok;
}

int main(int argc, char* argv[]) {

  const char * url = "dig2://192.168.0.254/";
  if (argc > 1) url = argv[1];

  printf("######################################################\n");
  printf("  Register Read/Write/Save/Load Test (ALL registers)\n");
  printf("  URL: %s\n", url);
  printf("######################################################\n\n");

  Digitizer2Gen * digi = new Digitizer2Gen();

  int ret = digi->OpenDigitizer(url);
  if (ret != 0) {
    printf("FAIL: Cannot open digitizer at %s (ret=%d)\n", url, ret);
    delete digi;
    return 1;
  }

  printf("Digitizer opened: SN=%d, Model=%s, FPGA=%s, Channels=%d\n\n",
         digi->GetSerialNumber(), digi->GetModelName().c_str(),
         digi->GetFPGAType().c_str(), digi->GetNChannels());

  bool t1 = TestReadAllSettings(digi);
  bool t2 = TestAllChannelRegisters(digi);
  bool t3 = TestAllBoardRegisters(digi);
  bool t4 = TestLVDSRegisters(digi);
  bool t5 = TestWriteAllChannels(digi);
  bool t6 = TestSaveAndLoad(digi);
  bool t7 = TestSaveLoadRoundTrip(digi);

  printf("######################################################\n");
  printf("  RESULTS\n");
  printf("  Test 1 (ReadAll):              %s\n", t1 ? "PASS" : "FAIL");
  printf("  Test 2 (Ch RW registers):      %s\n", t2 ? "PASS" : "FAIL");
  printf("  Test 3 (Board RW registers):   %s\n", t3 ? "PASS" : "FAIL");
  printf("  Test 4 (LVDS registers):       %s\n", t4 ? "PASS" : "FAIL");
  printf("  Test 5 (Write-all channels):   %s\n", t5 ? "PASS" : "FAIL");
  printf("  Test 6 (Save/Load verify):     %s\n", t6 ? "PASS" : "FAIL");
  printf("  Test 7 (Round-trip compare):   %s\n", t7 ? "PASS" : "FAIL");
  printf("######################################################\n");

  digi->CloseDigitizer();
  delete digi;

  return (t1 && t2 && t3 && t4 && t5 && t6 && t7) ? 0 : 1;
}

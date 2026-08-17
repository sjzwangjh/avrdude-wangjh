# picdude Phase 0 check report (after sync)

## 1. Final counts
| file | entries |
|---|---|
| g_deviceTable (picDeviceConst.c) | 274 |
| pic10-12-16-init.xml | 274 |
| pic_devicenames.inc | 274 |
| picdude.conf | 274 |

## 2. Original source state (before sync)
- g_deviceTable rows: 457; distinct: 286
- Duplicate groups: 129; extra rows: 183
- Removed by decision (PS200 + non-standard names): 12 -> PS200, PICRF675F, PICRF675H, PICRF675K, PICRF509AF, PICRF509AG, PIC16F1829LIN, PIC12F529T39A, PIC12F529T48A, PIC12LF1840T39A, PIC12LF1840T48A, PIC16LF1824T39A

## 3. Duplicate groups in original g_deviceTable (first occurrence kept)
| name | count | original row indices | identical rows |
|---|---|---|---|
| PIC10F200 | 2 | 4,25 | yes |
| PIC10F202 | 2 | 5,26 | yes |
| PIC10F204 | 2 | 6,27 | yes |
| PIC10F206 | 2 | 7,28 | yes |
| PIC10F220 | 2 | 10,29 | yes |
| PIC10F222 | 2 | 11,30 | yes |
| PIC10F320 | 2 | 19,31 | yes |
| PIC10F322 | 2 | 20,32 | yes |
| PIC10LF320 | 2 | 21,33 | yes |
| PIC10LF322 | 2 | 22,34 | yes |
| PIC12F1501 | 2 | 191,297 | yes |
| PIC12F1612 | 2 | 285,300 | yes |
| PIC12F1822 | 3 | 166,229,301 | yes |
| PIC12F1840 | 3 | 167,230,302 | yes |
| PIC12F508 | 2 | 8,35 | yes |
| PIC12F509 | 2 | 9,36 | yes |
| PIC12F510 | 2 | 12,37 | yes |
| PIC12F519 | 2 | 18,38 | yes |
| PIC12F609 | 2 | 13,41 | yes |
| PIC12F615 | 2 | 14,42 | yes |
| PIC12F617 | 2 | 15,43 | yes |
| PIC12F629 | 2 | 0,44 | yes |
| PIC12F635 | 2 | 2,45 | yes |
| PIC12F675 | 2 | 1,46 | yes |
| PIC12F683 | 2 | 3,47 | yes |
| PIC12F752 | 2 | 23,48 | yes |
| PIC12HV609 | 2 | 16,49 | yes |
| PIC12HV615 | 2 | 17,50 | yes |
| PIC12HV752 | 2 | 24,51 | yes |
| PIC12LF1501 | 2 | 196,303 | yes |
| PIC12LF1612 | 2 | 286,307 | yes |
| PIC12LF1822 | 3 | 173,231,308 | yes |
| PIC12LF1840 | 3 | 174,232,309 | yes |
| PIC16F1454 | 2 | 201,312 | yes |
| PIC16F1455 | 2 | 202,313 | yes |
| PIC16F1459 | 2 | 203,314 | yes |
| PIC16F1503 | 2 | 192,315 | yes |
| PIC16F1507 | 2 | 193,316 | yes |
| PIC16F1508 | 2 | 194,317 | yes |
| PIC16F1509 | 2 | 195,318 | yes |
| PIC16F1613 | 2 | 287,331 | yes |
| PIC16F1614 | 2 | 288,332 | yes |
| PIC16F1615 | 2 | 289,333 | yes |
| PIC16F1618 | 2 | 290,334 | yes |
| PIC16F1619 | 2 | 291,335 | yes |
| PIC16F1703 | 2 | 263,336 | yes |
| PIC16F1704 | 2 | 264,337 | yes |
| PIC16F1705 | 2 | 265,338 | yes |
| PIC16F1707 | 2 | 266,339 | yes |
| PIC16F1708 | 2 | 267,340 | yes |
| PIC16F1709 | 2 | 268,341 | yes |
| PIC16F1713 | 2 | 269,342 | yes |
| PIC16F1716 | 2 | 270,343 | yes |
| PIC16F1717 | 2 | 271,344 | yes |
| PIC16F1718 | 2 | 272,345 | yes |
| PIC16F1719 | 2 | 273,346 | yes |
| PIC16F1782 | 3 | 219,249,356 | yes |
| PIC16F1783 | 3 | 220,250,357 | yes |
| PIC16F1784 | 3 | 221,251,358 | yes |
| PIC16F1786 | 3 | 222,252,359 | yes |
| PIC16F1787 | 3 | 223,253,360 | yes |
| PIC16F1788 | 2 | 254,361 | yes |
| PIC16F1789 | 2 | 255,362 | yes |
| PIC16F1823 | 3 | 168,233,363 | yes |
| PIC16F1824 | 3 | 169,234,364 | yes |
| PIC16F1825 | 3 | 170,235,365 | yes |
| PIC16F1826 | 3 | 180,236,366 | yes |
| PIC16F1827 | 3 | 181,237,367 | yes |
| PIC16F1828 | 3 | 171,238,368 | yes |
| PIC16F1829 | 3 | 172,239,369 | yes |
| PIC16F1847 | 3 | 182,240,371 | yes |
| PIC16F1933 | 3 | 154,207,372 | yes |
| PIC16F1934 | 3 | 155,208,373 | yes |
| PIC16F1936 | 3 | 156,209,374 | yes |
| PIC16F1937 | 3 | 157,210,375 | yes |
| PIC16F1938 | 3 | 158,211,376 | yes |
| PIC16F1939 | 3 | 159,212,377 | yes |
| PIC16LF1454 | 2 | 204,380 | yes |
| PIC16LF1455 | 2 | 205,381 | yes |
| PIC16LF1459 | 2 | 206,382 | yes |
| PIC16LF1503 | 2 | 197,383 | yes |
| PIC16LF1507 | 2 | 198,384 | yes |
| PIC16LF1508 | 2 | 199,385 | yes |
| PIC16LF1509 | 2 | 200,386 | yes |
| PIC16LF1613 | 2 | 292,403 | yes |
| PIC16LF1614 | 2 | 293,404 | yes |
| PIC16LF1615 | 2 | 294,405 | yes |
| PIC16LF1618 | 2 | 295,406 | yes |
| PIC16LF1619 | 2 | 296,407 | yes |
| PIC16LF1703 | 2 | 274,408 | yes |
| PIC16LF1704 | 2 | 275,409 | yes |
| PIC16LF1705 | 2 | 276,410 | yes |
| PIC16LF1707 | 2 | 277,411 | yes |
| PIC16LF1708 | 2 | 278,412 | yes |
| PIC16LF1709 | 2 | 279,413 | yes |
| PIC16LF1713 | 2 | 280,414 | yes |
| PIC16LF1716 | 2 | 281,415 | yes |
| PIC16LF1717 | 2 | 282,416 | yes |
| PIC16LF1718 | 2 | 283,417 | yes |
| PIC16LF1719 | 2 | 284,418 | yes |
| PIC16LF1782 | 3 | 224,256,428 | yes |
| PIC16LF1783 | 3 | 225,257,429 | yes |
| PIC16LF1784 | 3 | 226,258,430 | yes |
| PIC16LF1786 | 3 | 227,259,431 | yes |
| PIC16LF1787 | 3 | 228,260,432 | yes |
| PIC16LF1788 | 2 | 261,433 | yes |
| PIC16LF1789 | 2 | 262,434 | yes |
| PIC16LF1823 | 3 | 175,241,435 | yes |
| PIC16LF1824 | 3 | 176,242,436 | yes |
| PIC16LF1825 | 3 | 177,243,438 | yes |
| PIC16LF1826 | 3 | 183,244,439 | yes |
| PIC16LF1827 | 3 | 184,245,440 | yes |
| PIC16LF1828 | 3 | 178,246,441 | yes |
| PIC16LF1829 | 3 | 179,247,442 | yes |
| PIC16LF1847 | 3 | 185,248,443 | yes |
| PIC16LF1902 | 2 | 186,444 | yes |
| PIC16LF1903 | 2 | 187,445 | yes |
| PIC16LF1904 | 2 | 188,446 | yes |
| PIC16LF1906 | 2 | 189,447 | yes |
| PIC16LF1907 | 2 | 190,448 | yes |
| PIC16LF1933 | 3 | 160,213,449 | yes |
| PIC16LF1934 | 3 | 161,214,450 | yes |
| PIC16LF1936 | 3 | 162,215,451 | yes |
| PIC16LF1937 | 3 | 163,216,452 | yes |
| PIC16LF1938 | 3 | 164,217,453 | yes |
| PIC16LF1939 | 3 | 165,218,454 | yes |
| PICRF675F | 2 | 52,150 | yes |
| PICRF675H | 2 | 53,151 | yes |
| PICRF675K | 2 | 54,152 | yes |

## 4. Capacity suspects kept (code_end_addr == 0x800; not corrected per user decision)
Count: 207

## 5. Output
- E:\wangjunhua\Project\AvrProgrammer\avrdude\src\picdude.conf : 274 part entries; XML-missing: 0 ()
- This report: E:\wangjunhua\Project\AvrProgrammer\avrdude\src\picdude_check_report.md
- Backups: picDeviceConst.full.c / picDeviceConst.full.h next to the originals;
  pic10-12-16-init.xml and pic_devicenames.inc are recoverable via git.


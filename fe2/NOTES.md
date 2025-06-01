### Notes on m68kram:

###### From host.h
```c
#define LOAD_BASE 0x0
#define MEM_SIZE (0x110000)

union Reg
{
	u16 word[2];
	u32 _u32;
	u16 _u16;
	u8 _u8;
	s32 _s32;
	s16 _s16;
	s8 _s8;
};

/* m68000 state ----------------------------------------------------- */
extern s8 m68kram[MEM_SIZE];
extern union Reg Regs[16];
```

### Notes on editing the cash value

* the cash value and other associated values show up twice in the entirety of the RAM — the first seems to be in general and is the real-time reflection of the value. the second is when save data is being loaded or saved.  
  what's interesting is that the value is always denoted by the hex pattern `FF FF FF FF A9 25 C5` — in decimal that's `255 255 255 255 169`.  
  the cash value is always between that and the `@` symbol aka `0x40`.
* 4 hex spots control the total sum off the cash value. starting after `FF FF FF FF A9 25 C5` the first offset at spot 6 if set to 1 (hex `01`) the value starting from cash value 0.0 is
1677721.6 spot 7 is 6553.6 spot 8 is 25.6 and spot 9 is 0.1

---

### Notes on Potential Save Data Location in M68K RAM (`fe2.s.c`)

#### Observations

* `Regs[10]._s32 = Regs[12]._s32;` — appears **after** `__NL73164_EncryptSaveData:` → strong candidate for the start of raw save data **before encryption**.
* same line appears again **after** `__NL73180_DecryptSaveData:` → strong candidate for the start of raw save data **after encryption**.
* "There are 4 hexadecimal spots that control the total cash value. Starting from the offset after `FF FF FF FF A9 25 C5`, the values at the following  spots have the following effects with the cash value being reset to 0.0 each time:"

* **(offset 6)**: If set to `01`, the cash value increases to 1677721.6.
* **(offset 7)**: If set to `01`, the cash value increases by 6553.6.
* **(offset 8)**: If set to `01`, the cash value increases by 25.6.
* **(offset 9)**: If set to `01`, the cash value increases by 0.1.
* So understanding this a million would be 0x00 0x98 0x96 0x80 or in decimal 0 98 96 80

#### Conclusion

the fact that `Regs[10]` changes and reflects both encrypted and unencrypted save data right at the start of the process suggests this is likely the save data address.

---

### Notes on the save data from below

* at `00000f40: SJameson.._` → you can change `SJameson` to something like `Bameson` and it will reflect in the save-to-disk menu.  
  same goes for `00000c30: !JAMESON` — it'll reflect in the commander info menu.

* `..TY-198..r..(` — this seems to be the ship tag. it can be changed too, although it shows up in multiple places, only one instance actually affects the in-game display on the ship.

* the cash and cargo values are **right above** the uppercase `JAMESON`.  
  there’s a good chance this sequence marks the start:  
  `255 255 255 255 169 37` → in hex: `FF FF FF FF A9 25`.

---

#### you can comment out the below in `fe2.s.c` and the save data will be saved **unencrypted** upon saving  
however, loading the save will fail.

```c
	wrlong (Regs[15]._s32, __D67706);
	{
		// goto __NL73164_EncryptSaveData;
	}
```

#### typical save file that is not encrypted by __NL73164_EncryptSaveData;

```hex
00000000: 7144 0011 0064 2a2a 2a4f 4e02 4f1d 1d0f  qD...d***ON.O...
00000010: 1d1d 1d1d 00ff 00ff 00ff 00ff 00ff 00ff  ................
00000020: 00ff 00ff 00ff 00ff 00ff 00ff 00ff 00ff  ................
00000030: 00ff 00ff 00ff 00ff 00ff 00ff 00ff 00ff  ................
00000040: 00ff 00ff 00ff 00ff 00ff 00ff 00ff 00ff  ................
00000050: 00ff 00ff 00ff 00ff 00ff 00ff 00ff 00ff  ................
00000060: 00ff 00ff 00ff 00ff 00ff 00ff 00ff 00ff  ................
00000070: 00ff 00ff 00ff 00ff 00ff 00ff 00ff 00ff  ................
00000080: 00ff 00ff 00ff 00ff 00ff 00ff 00ff 00ff  ................
00000090: 00ff 00ff 00ff 00ff 00ff 00ff 00ff 00ff  ................
000000a0: 00ff 00ff 00ff 00ff 00ff 00ff 00ff 00ff  ................
000000b0: 00ff 00ff 00ff 00ff 00ff 00ff 00ff 00ff  ................
000000c0: 00ff 00ff 00ff 00ff 00ff 00ff 00ff 00ff  ................
000000d0: 00ff 00ff 00ff 00ff 00ff 00ff 00ff 00ff  ................
000000e0: 00ff 00ff 00ff 00ff 00ff 00ff 00ff 00ff  ................
000000f0: 00ff 00ff 0018 dd02 1615 8801 78dd ff16  ............x...
00000100: dc09 f8f1 82ed eb14 3c00 1834 deed 2ffe  ........<..4../.
00000110: ee92 d6d4 aaa0 5300 03ff ffff ffff ffff  ......S.........
00000120: fe6f ff6d 1cf4 f03e a63f 2413 1c93 bc00  .o.m...>.?$.....
00000130: 017d 1616 1400 0001 0000 c63d 650b 0b00  .}.........=e...
00000140: 0d80 0000 8000 0146 ffff 9eb2 30f7 ffff  .......F....0...
00000150: 0001 8000 00e8 f800 0011 d5a0 000b 154a  ...............J
00000160: eab6 154a eab6 3539 eab6 0001 4024 0003  ...J..59....@$..
00000170: 0201 8800 0282 8200 0a64 0018 5400 0002  .........d..T...
00000180: 0015 ff00 0420 0003 592d 3139 3800 0072  ..... ..Y-198..r
00000190: 001f 83c0 1544 027e fdc1 018d 80fd eadf  .....D.~........
000001a0: 82cc fedb 3c00 1833 e440 a501 decb 91d4  ....<..3.@......
000001b0: 7fcb 8100 07ff ffff fe6f ff6d 1cf4 f03e  .........o.m...>
000001c0: a63f 2413 1c93 bc00 017d 1616 1400 0001  .?$......}......
000001d0: 0000 c630 660b 0b00 0d02 8100 0056 0000  ...0f........V..
000001e0: 46ff ff03 246d 7bff ff00 0180 0000 b67e  F...$m{........~
000001f0: 0000 11d5 a000 0b15 4aea b615 4aea b635  ........J...J..5
00000200: 39ea b600 0140 2400 0302 0188 0002 8282  9....@$.........
00000210: 000a 6400 1854 0000 0200 15ff 0004 2000  ..d..T........ .
00000220: 0359 2d31 3938 0000 7200 1f7a 2d16 24e9  .Y-198..r..z-.$.
00000230: 9816 97ff f47d 0815 9f82 edfb fd3c 0018  .....}.......<..
00000240: 34ee 754c fff8 20f8 d4ac dfe5 0003 ffff  4.uL.. .........
00000250: ffff ffff fffe 6fff 6d1c f4f0 3ea6 3f24  ......o.m...>.?$
00000260: 131c 93bc 0001 7d16 1614 0000 0100 00c6  ......}.........
00000270: 0d67 0b07 000d 8000 0080 0001 46ff ff57  .g..........F..W
00000280: 92ea c0ff ff00 0180 0000 2e98 0000 11d5  ................
00000290: a000 0b15 4aea b615 4aea b635 39ea b600  ....J...J..59...
000002a0: 0140 2400 0302 0188 0002 8282 000a 6400  .@$...........d.
000002b0: 1854 0000 0200 15ff 0004 2000 0359 2d31  .T........ ..Y-1
000002c0: 3938 0000 7200 0c99 ce00 0399 ce00 0a7f  98..r...........
000002d0: 0006 7f00 067f 0000 3c55 4400 02d0 0002  ........<UD.....
000002e0: 47ff ff6e fffc 709a fffd 0f53 0000 01ea  G..n..p....S....
000002f0: ca44 0002 d000 0247 ffff 6eff fc70 9aff  .D.....G..n..p..
00000300: fd0f 5300 0001 eaca 7200 152c 0000 680b  ..S.....r..,..h.
00000310: 247f 0008 ffff ffff 8000 0080 0001 4672  $.............Fr
00000320: 4689 445e 8700 0380 011b 7200 0011 d5a0  F.D^......r.....
00000330: 000b 1108 eef8 1108 eef8 2652 eef8 0001  ..........&R....
00000340: 4008 0003 0301 9100 0fdc 0009 ffb7 000b  @...............
00000350: 6b10 0000 0108 1703 fd08 1700 0e03 fb07  k...............
00000360: 0003 4000 0245 572d 3530 3500 0072 000d  ..@..EW-505..r..
00000370: 445c 0003 1b58 0003 8012 0005 1615 82e7  D\...X..........
00000380: 7e12 0005 82e8 e9eb 3c00 1834 db68 98ff  ~.......<..4.h..
00000390: fc18 10d4 a935 2000 03ff ffff ffff ffff  .....5 .........
000003a0: fe6f ff00 1436 0000 690b 0600 0d80 0000  .o...6..i.......
000003b0: 8000 0146 ffff f2eb 8def 0003 5c1d e560  ...F........\..`
000003c0: 0000 11d5 a300 0b15 4aea b615 4aea b62e  ........J...J...
000003d0: d6ea b600 0140 0400 0303 0291 0002 8200  .....@..........
000003e0: 0a01 e000 176e 8400 0002 0014 3501 0700  .....n......5...
000003f0: 0340 0002 4b4f 2d37 3531 0000 7200 0c01  .@..KO-751..r...
00000400: 86a0 0003 4e20 0003 8000 047f 0006 7f00  ....N ..........
00000410: 067f 0000 3c00 1820 2000 010b 6000 01b3  ....<..  ...`...
00000420: 6000 01ff ff24 25ff fb62 efff fe46 8500  `....$%..b...F..
00000430: 1648 0000 6a0b ff00 0d80 0000 8000 02ff  .H..j...........
00000440: ffe3 9b6a 6f00 03d1 9cba a000 0011 d5a0  ...jo...........
00000450: 000b 0663 f99d 0663 f99d 0cc6 f99d 0001  ...c...c........
00000460: 400c 0003 0704 b300 0282 8282 8282 8200  @...............
00000470: 04c0 0fa0 000b a925 c5c0 02d0 2000 0105  .......%.... ...
00000480: 44fa 0000 3c6e 0014 018f 0134 0003 c000  D...<n.....4....
00000490: 024b 502d 3037 3000 0072 000d d6d8 0003  .KP-070..r......
000004a0: 55f0 0003 8000 0616 1582 e77e 1200 0582  U..........~....
000004b0: e8e9 eb3c 0018 34db 5e88 0000 03e7 e8d4  ...<..4.^.......
000004c0: a96e 1000 07ff ffff fe6f ff6d 1cf4 f03e  .n.......o.m...>
000004d0: a63f 2413 1c93 bc00 017d 1616 1400 0220  .?$......}.....
000004e0: 556b 0300 0e80 0000 8000 0146 ffff 4042  Uk.........F..@B
000004f0: 8dfb 0003 8001 1b72 0000 11d5 a000 0b15  .......r........
00000500: 4aea b615 4aea b635 39ea b600 0140 2400  J...J..59....@$.
00000510: 0302 0188 0002 8282 000a 6400 1884 0000  ..........d.....
00000520: 0200 15ff 0004 2000 0254 592d 3139 3800  ...... ..TY-198.
00000530: 0072 000d 28a0 0003 1450 0003 8000 0481  .r..(....P......
00000540: ee16 1300 0116 137e 1300 0580 023c 550f  .......~.....<U.
00000550: 4800 013b 7800 01c4 e000 01ff fe77 2a00  H..;x........w*.
00000560: 0124 3f00 0138 fb0f 4800 013b 7800 01c4  .$?..8..H..;x...
00000570: e000 01ff fe77 2a00 0124 3f00 0138 fb72  .....w*..$?..8.r
00000580: 0015 7e00 006c 0123 000d 8000 0080 0001  ..~..l.#........
00000590: 46ff ff2b 0415 ba74 ea00 0018 8001 1b72  F..+...t.......r
000005a0: 0000 11d5 a063 a900 0031 fff8 0000 01ff  .....c...1......
000005b0: ce5e 56bb 803a 27ad ec00 0001 6578 03c4  .^V..:'.....ex..
000005c0: 0100 0101 071c 000c 1800 0458 003d 526f  ...........X.=Ro
000005d0: 7373 2031 3534 2033 0009 8582 0003 8582  ss 154 3........
000005e0: 000b bfe5 6ec8 0001 6ec7 401c 0005 8002  ....n...n.@.....
000005f0: 3c55 0000 6096 0000 ea19 ae00 000b bba8  <U..`...........
00000600: 0000 ffff fe47 0002 0dff ffd2 76b8 4496  .....G......v.D.
00000610: 0000 d6dd ae00 0004 67a8 0003 17ff ffff  ........g.......
00000620: f7ff ffff c670 0015 7e00 006d 0119 000d  .....p..~..m....
00000630: 8000 0080 0001 46ff ff52 2ffe e270 e200  ......F..R/..p..
00000640: 0016 8001 1b72 0000 11d5 a07e 5c00 0026  .....r.....~\..&
00000650: 0000 2bff f100 0014 5248 0001 90a2 b5e0  ..+.....RH......
00000660: 1488 8000 000d 8201 0001 022a aa00 0c16  ...........*....
00000670: 0004 e900 3d57 696c 6c6f 7700 0034 000b  ....=Willow..4..
00000680: 8582 0003 8582 000d 1643 81f5 7ffe 0005  .........C......
00000690: 81f7 e9bc 3c3e fab8 8c70 6f1b 17d0 89b3  ....<>...po.....
000006a0: dea0 ffff fe06 0002 2dff ffd2 9834 df33  ........-....4.3
000006b0: c000 03d4 aa30 0800 07ff ffff fe6f ff00  .....0.......o..
000006c0: 1462 0000 6e01 0600 0d80 0000 8000 0146  .b..n..........F
000006d0: ffff 5c16 a57c 7ffa ffd5 8001 1b72 0000  ..\..|.......r..
000006e0: 11d5 a000 0415 fff4 ffd4 0003 53e6 0005  ............S...
000006f0: 1555 0002 0300 0201 6b69 0007 ffd9 0000  .U......ki......
00000700: 1000 0101 2500 3d53 6972 6f63 636f 2053  ....%.=Sirocco S
00000710: 7461 7469 6f6e 0005 44aa 0003 44aa 0004  tation..D...D...
00000720: f000 04bf e56e c800 016e c740 1c00 0580  .....n...n.@....
00000730: 023c 5515 4617 0000 4152 8400 005e 31a0  .<U.F...AR...^1.
00000740: 0000 ffff fe07 0002 2dff ffd2 97cd 2a17  ........-.....*.
00000750: 0000 2e16 8400 0056 dda0 0000 ffff ffd7  .......V........
00000760: 0002 17ff ffff e770 0015 8c00 006f 0113  .......p.....o..
00000770: 000e 9f13 2f00 0046 ffff addd aabd 64ca  ..../..F......d.
00000780: 0000 1880 011b 7200 0011 d5a0 691c 0000  ......r.....i...
00000790: 2600 0015 fff4 ffd4 0de8 0001 53e6 efcc  &...........S...
000007a0: 1b12 8000 0015 5501 0001 022a aa00 0c18  ......U....*....
000007b0: 0003 010d 003d 4d65 726c 696e 0000 3400  .....=Merlin..4.
000007c0: 0b2f f800 0201 3304 0002 0100 0778 4e2b  ./....3......xN+
000007d0: ac00 01d4 5578 5000 057f fe3c 5548 1c00  ....UxP....<UH..
000007e0: 0113 3c00 0107 5400 01ff fffe 2f00 0216  ..<...T...../...
000007f0: ffff d2b0 481c 0001 133c 0001 0754 0001  ....H....<...T..
00000800: ffff fe2f 0002 16ff ffd2 b072 0015 9400  .../.......r....
00000810: 0070 0118 000d 0126 fff6 0000 46ff ff8d  .p.....&....F...
00000820: 4078 ff40 9200 0024 8001 1b72 0000 11d5  @x.@...$...r....
00000830: a05a b400 002e 0000 32ff feff fe56 0440  .Z......2....V.@
00000840: 0000 7e5d 9142 0000 2464 0000 01fd 0300  ..~].B..$d......
00000850: 0101 0e38 000c 2400 04fd 003d 4173 7465  ...8..$....=Aste
00000860: 7200 0035 3400 0b19 c000 0207 2c00 0307  r..54.......,...
00000870: 0007 7ffc 0005 7ffe 0005 7ffe 3c55 49c2  ............<UI.
00000880: 8000 008d a300 01c2 3d00 01ff fff4 a400  ........=.......
00000890: 02b2 0001 0774 49c2 8000 008d a300 01c2  .....tI.........
000008a0: 3d00 01ff fff4 a400 02b2 0001 0774 7200  =............tr.
000008b0: 1580 0000 7101 2000 0d80 0000 8000 0146  ....q. ........F
000008c0: ffff 24ae b45a 50a2 0000 1980 011b 7200  ..$..ZP.......r.
000008d0: 0011 d5a0 6cd9 0000 2cff e400 0002 ffd6  ....l...,.......
000008e0: 3ff4 8000 0028 5e27 aa00 00dd 8800 0002  ?....(^'........
000008f0: 8f00 0201 000e 1900 04de 003d 4475 7374  ...........=Dust
00000900: 2042 616c 6c00 0a59 6800 0359 6800 0b7f   Ball..Yh..Yh...
00000910: 0006 7f00 067f 0000 3c55 0046 9a00 0072  ........<U.F...r
00000920: 0120 000d 8000 0080 0001 46ff ffa6 e7e4  . ........F.....
00000930: 364a f800 002c 8001 1b72 0000 11d5 a000  6J...,...r......
00000940: 0212 0013 0200 0235 5500 0c2c 0003 0d91  .......5U..,....
00000950: 003d 526f 7373 2031 3534 000b fa00 03a2  .=Ross 154......
00000960: 9c00 03a4 0001 0900 c200 0000 0000 0000  ................
00000970: 5000 5000 8800 03a9 5a00 0000 0000 03f7  P.P.....Z.......
00000980: 3400 0000 0000 0762 faf2 0200 0000 055b  4......b.......[
00000990: 62f1 677f 0000 0637 0a00 0bff 0000 03f8  b.g....7........
000009a0: c2f2 0000 0000 0405 b400 0000 0000 0537  ...............7
000009b0: d200 0a00 0000 059f 9800 0000 0000 054a  ...............J
000009c0: ec00 0000 0000 0668 42fe 00ff 0000 0689  .......hB.......
000009d0: 1a00 1600 0000 06a2 58f4 0a00 0000 0445  ........X......E
000009e0: 0e00 0000 0000 0725 7af3 3a00 0000 074e  .......%z.:....N
000009f0: 3200 0200 0000 0754 0c68 0000 0000 0759  2......T.h.....Y
00000a00: 2200 0000 0000 0000 0000 0000 0000 0000  "...............
00000a10: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000a20: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000a30: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000a40: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000a50: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000a60: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000a70: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000a80: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000a90: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000aa0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000ab0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000ac0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000ad0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000ae0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000af0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000b00: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000b10: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000b20: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000b30: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000b40: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000b50: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000b60: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000b70: 0000 0000 0080 011b 7200 0011 d5a0 8000  ........r.......
00000b80: 0094 ae00 00ff a200 0101 0000 0200 01ff  ................
00000b90: ffa0 2ebc 1761 d7cb b900 007b 7b1a 1b49  .....a.....{{..I
00000ba0: fb8a 8480 e6fe fa7d 0000 7d81 1000 0107  .......}..}.....
00000bb0: 0000 0700 0007 0000 0600 0005 0000 0400  ................
00000bc0: 0003 0000 0200 01f8 0000 0a00 0003 0005  ................
00000bd0: ffff ffff 0003 fd1f 0000 2d82 c800 0b05  ..........-.....
00000be0: 1e00 0106 0103 0200 070a 6e49 c000 000e  ..........nI....
00000bf0: f0ae 0100 00a9 25c5 c100 0080 0001 0100  ......%.........
00000c00: 0103 9458 6b00 006e 0001 0148 9e00 0001  ...Xk..n...H....
00000c10: 4bf8 0001 0200 1260 0009 0100 05ff ffff  K......`........
00000c20: ffa9 25c5 c100 0503 e800 0004 0002 0100  ..%.............
00000c30: 0302 0000 ff40 0028 0100 1503 2000 214a  .....@.(.... .!J
00000c40: 414d 4553 4f4e 000a 01f3 01f3 0000 11d5  AMESON..........
00000c50: 9f00 0011 d59f 0009 6e01 154a eab6 154a  ........n..J...J
00000c60: eab6 3539 eab6 0002 3f00 0317 176a 0000  ..59....?....j..
00000c70: 1524 3200 0102 22e0 0008 6117 176a 0000  .$2..."...a..j..
00000c80: 1524 3200 00ff ffcc 0000 1717 6a00 0015  .$2.........j...
00000c90: 2432 0000 ffff cc00 0052 6f73 7320 3135  $2.......Ross 15
00000ca0: 3400 0954 7970 6527 4727 7965 6c6c 6f77  4..Type'G'yellow
00000cb0: 2073 7461 7200 2e88 000b 0a6e 49c0 a925   star......nI..%
00000cc0: c5c1 84ed 800b 0000 8000 0601 0002 ff00  ................
00000cd0: 646f 0003 8000 0311 d5a0 7fff 0000 2a00  do............*.
00000ce0: 030b 0b04 070f 0000 0381 0203 0c89 8104  ................
00000cf0: 0503 0102 0a05 0181 8181 0301 0001 0402  ................
00000d00: 0200 0104 0002 0100 0003 0000 8000 0002  ................
00000d10: 0000 015c 16a5 7c12 4600 000a 049e 0000  ...\..|.F.......
00000d20: 5800 0064 01bb 0001 04f0 0199 03b1 034c  X..d...........L
00000d30: 0000 c900 002c 10b9 0001 3065 0000 4516  .....,....0e..E.
00000d40: 8c03 3900 009e 0000 481a 6b00 0003 49e5  ..9.....H.k...I.
00000d50: 0000 014f 3800 013c 3600 0030 018c 0306  ...O8..<6..0....
00000d60: 0111 0000 2842 e500 0001 790b 159a 0000  ....(B....y.....
00000d70: 4403 2d00 0073 0000 8b01 ee00 0127 8b00  D.-..s.......'..
00000d80: 012f 4200 0140 a705 2b05 3900 0053 115b  ./B..@..+.9..S.[
00000d90: 0000 3b07 f901 9202 f700 0122 a600 00e2  ..;........"....
00000da0: ffd5 038b fff9 6e04 9819 9903 02bc 0a0c  ......n.........
00000db0: a915 c5c0 412b 47b0 981a 9903 021c 0000  ....A+G.........
00000dc0: 08a9 25c5 c61f dcd2 5a98 1c00 0001 0000  ..%.....Z.......
00000dd0: 7800 01a9 1dc5 8369 eaed 5d98 298e 0d78  x......i..].)..x
00000de0: 6c00 040d f55f 5ff5 981a 9904 0384 0410  l....__.........
00000df0: a925 c5c0 81f1 f8d4 9826 9904 0384 0410  .%.......&......
00000e00: 5727 8b5d 81f1 f8d4 9829 8e16 5e84 0410  W'.].....)..^...
00000e10: 0002 1637 7c2d 8798 1a99 0405 7800 0014  ...7|-......x...
00000e20: a925 c582 b783 5973 9819 9903 0370 0000  .%....Ys.....p..
00000e30: 08a9 25c5 823d 33ad 7598 1999 0304 b000  ..%..=3.u.......
00000e40: 0008 a925 c600 0042 21c5 8f98 1999 0303  ...%...B!.......
00000e50: 2000 000c a91d c5c0 c2b5 2fd9 981a 9905   ........./.....
00000e60: 0208 2c1c a925 c5c6 bdbe 3769 0060 0c00  ..,..%....7i.`..
00000e70: 0036 e08d 0000 48cb bf00 0036 7794 0000  .6....H....6w...
00000e80: 3e9a 5400 2804 00ff 00ff 00ff 00ff 00ff  >.T.(...........
00000e90: 00ff 00ff 00ff 00ff 00ff 00ff 00ff 00ff  ................
00000ea0: 00ff 00ff 00ff 00ff 00ff 00ff 00ff 00ff  ................
00000eb0: 00ff 00ff 00ff 00ff 00ff 00ff 00ff 00ff  ................
00000ec0: 00ff 00ff 007a 0800 0598 53a9 25c5 c600  .....z....S.%...
00000ed0: 0011 d5b6 0002 faff ff99 595e 38a6 0300  ..........Y^8...
00000ee0: 0198 53a9 2dc5 c000 0011 d5b5 0001 012c  ..S.-..........,
00000ef0: ffff 9958 29c8 f4cf 00ff 00ff 003c 0200  ...X)........<..
00000f00: 16ff ff00 00ff ffff 0002 ff00 00ff ff00  ................
00000f10: 02ff ffff ff00 01ff ffff ffff ffff ff00  ................
00000f20: 00ff 0300 0401 00ff 00ff 00ff 00ff 00ff  ................
00000f30: 00ff 00ff 00ff 00ff 00ff 00ff 00ff 00ff  ................
00000f40: 00ff 00ff 0053 4a61 6d65 736f 6e00 155f  .....SJameson.._
00000f50: 3031 000d 0000 ffff 0000 ffff ffff ffff  01..............
```


### Super useful functions used in host.h to find and work with the emulated memory

```c
static inline void scan_m68kram()
{
	FILE *f = fopen("m68kram_scan.txt", "w");
	fprintf(f, "Scanning m68kram (0x0-0x%08X) for credits (9100) and Jameson\n", MEM_SIZE);

// Store Jameson match addresses
#define MAX_MATCHES 128
	u32 jameson_matches[MAX_MATCHES];
	int jameson_count = 0;

	for (u32 pos = 0; pos < MEM_SIZE - 8; pos++)
	{
		if (rdbyte(pos) == 0x4A && rdbyte(pos + 1) == 0x61 && rdbyte(pos + 2) == 0x6D &&
			rdbyte(pos + 3) == 0x65 && rdbyte(pos + 4) == 0x73 && rdbyte(pos + 5) == 0x6F &&
			rdbyte(pos + 6) == 0x6E && rdbyte(pos + 7) == 0x00)
		{
			fprintf(f, "Jameson found at emulator memory offset: 0x%08X\n", LOAD_BASE + pos);

			const char *replacement = "XXXXXXX";
			for (int i = 0; i < 7; i++)
				wrbyte(pos + i, replacement[i]);
			wrbyte(pos + 7, 0x00);
		}
	}

	// Find all "Jameson" matches
	for (u32 pos = 0; pos < MEM_SIZE - 8; pos++)
	{
		if (rdbyte(pos) == 0x4A && rdbyte(pos + 1) == 0x61 && rdbyte(pos + 2) == 0x6D &&
			rdbyte(pos + 3) == 0x65 && rdbyte(pos + 4) == 0x73 && rdbyte(pos + 5) == 0x6F &&
			rdbyte(pos + 6) == 0x6E && rdbyte(pos + 7) == 0x00)
		{
			fprintf(f, "Found Jameson at 0x%08X\n", pos);
			if (jameson_count < MAX_MATCHES)
			{
				jameson_matches[jameson_count++] = pos;
			}
		}
	}

	// Dump around each Jameson
	for (int i = 0; i < jameson_count; i++)
	{
		u32 addr = jameson_matches[i];
		u32 start = (addr >= 0x100) ? addr - 0x100 : 0;
		u32 end = (addr + 0x100 < MEM_SIZE) ? addr + 0x100 : MEM_SIZE;

		fprintf(f, "\nDump around Jameson (0x%08X ± 256 bytes):\n", addr);
		for (u32 pos = start; pos < end; pos++)
		{
			fprintf(f, "%02X ", rdbyte(pos));
			if ((pos - start) % 16 == 15)
				fprintf(f, "\n");
		}
		fprintf(f, "\n");
	}

	fclose(f);
}

static inline u8 rdbyte_offset(u32 base_addr, s32 offset)
{
	u32 target_addr = base_addr + offset;
	return rdbyte(target_addr);
}

static inline void print_byte_at_offset(u32 base_addr, s32 offset)
{
	u8 val = rdbyte(base_addr + offset);
	printf("Byte at address 0x%08X (offset %+d): 0x%02X (%c)\n", base_addr + offset, offset, val,
		   (val >= 0x20 && val <= 0x7E) ? val : '.');
}

static inline void dump_m68k_ram(u32 addr, u32 bytes)
{
	printf("Dumping 0x%08X bytes at address 0x%08X...\n", bytes, addr);

	u32 start = (addr >= bytes) ? addr - bytes : 0;
	u32 end = (addr + bytes < MEM_SIZE) ? addr + bytes : MEM_SIZE;

	const char *filename_format = "m68kram_dump_%x.txt";
	char filename[256];

	snprintf(filename, sizeof(filename), filename_format, addr);

	FILE *file = fopen(filename, "a+");
	if (file == NULL)
	{
		printf("Error opening file!\n");
		return;
	}

	fprintf(file, "\nDump of save data around address 0x%08X ± %u bytes:\n", addr, bytes);
	fprintf(file, "Bounds: 0x%08X - 0x%08X\n", start, end);
	fprintf(file, "Offset   Address   Hex Bytes                                 		 ASCII              Decimal Values\n");

	for (u32 pos = start; pos < end; pos += 16)
	{
		s32 offset = (s32)pos - (s32)addr;
		fprintf(file, "%+07d: %08X: ", offset, pos);

		for (int i = 0; i < 16; i++)
		{
			u8 byte = rdbyte(pos + i);
			fprintf(file, "%02X ", byte);
		}

		fprintf(file, " | ");

		for (int i = 0; i < 16; i++)
		{
			u8 byte = rdbyte(pos + i);
			fprintf(file, "%c", (byte >= 0x20 && byte <= 0x7E) ? byte : '.');
		}

		fprintf(file, " | ");

		for (int i = 0; i < 16; i++)
		{
			u8 byte = rdbyte(pos + i);
			fprintf(file, "%3d ", byte);
		}

		fprintf(file, "\n");
	}

	fprintf(file, "\n");
	fclose(file);

	printf("DONE!\n");
}

#include <ctype.h>	// for isxdigit, isspace
#include <stdlib.h> // for strtol

static inline void write_m68k_ram(u32 addr)
{
	printf("Writing 0x%08X bytes at address 0x%08X...\n", MEM_SIZE, addr);
	FILE *file = fopen("test.txt", "r");
	if (file == NULL)
	{
		printf("Could not open file\n");
		return;
	}

	char line[1024];
	u32 mem_pos = addr;
	while (fgets(line, sizeof(line), file))
	{
		// Extract the hexadecimal data (after second colon)
		char *hex_data = strstr(line, ":");
		if (hex_data != NULL)
		{
			hex_data = strstr(hex_data + 1, ":");
			if (hex_data != NULL)
			{
				hex_data += 1; // skip ":"
				char *ascii_start = strchr(hex_data, '|');
				if (ascii_start != NULL)
				{
					*ascii_start = '\0';
				}

				char byte[3] = {0}; // 2 hex digits + null terminator
				for (int i = 0; hex_data[i] != '\0';)
				{
					if (isspace(hex_data[i]))
					{
						i++;
						continue;
					}

					if (isxdigit(hex_data[i]) && isxdigit(hex_data[i + 1]))
					{
						byte[0] = hex_data[i];
						byte[1] = hex_data[i + 1];
						int value = (int)strtol(byte, NULL, 16);
						wrbyte(mem_pos, value);
						mem_pos++;
						i += 2;
					}
					else
					{
						i++;
					}
				}
			}
		}
	}

	fclose(file);

	printf("DONE!\n");
}

static inline void scan_m68k_ram_hex(u32 addr)
{
	const u8 expected_bytes[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xA9, 0x25};
	u32 start = addr;
	u32 len = sizeof(expected_bytes);
	for (u32 pos = start; pos < MEM_SIZE - len; pos++)
	{
		int match = 1;
		for (u32 i = 0; i < len; i++)
		{
			if ((u8)rdbyte(pos + i) != expected_bytes[i])
			{
				match = 0;
				break;
			}
		}
		if (match)
		{
			printf("\nfound\n");
			int tPos = 0;
			for (u32 offset = len;; offset++)
			{
				u8 byte = (u8)rdbyte(pos + offset);
				if (byte == 0x40)
				{
					printf("\nReached 0x64 at offset %u (address 0x%X), stopping collection.\n", offset, pos + offset);
					break;
				}
				if (tPos == 8)
				{
					wrbyte(pos + offset, 0x04);
				}

				printf("%02X at %d ", byte, tPos);
				tPos++;
			}
			printf("\n");
			break;
		}
	}
}

static inline void edit_cash_values()
{
	if (inject_cash_value)
	{
		printf("Injecting cash value !!!!!!\n");
		const u8 expected_bytes[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xA9, 0x25};
		u32 start = 0;
		u32 len = sizeof(expected_bytes);
		for (u32 pos = start; pos < MEM_SIZE - len; pos++)
		{
			int match = 1;
			for (u32 i = 0; i < len; i++)
			{
				if ((u8)rdbyte(pos + i) != expected_bytes[i])
				{
					match = 0;
					break;
				}
			}
			if (match)
			{
				int tPos = 0;
				for (u32 offset = len;; offset++)
				{
					u8 byte = (u8)rdbyte(pos + offset);
					if (byte == 0x40)
					{
						break;
					}
					if (tPos == 8)
					{
						wrbyte(pos + offset, 0x04);
					}
					tPos++;
				}
				break;
			}
		}
		inject_cash_value = 0;
	}
}

extern int dump_m68k_toggle;
static inline void dump_all_m68k_ram()
{
	if (dump_m68k_toggle)
	{
		printf("Dumping m68k RAM please wait...\n");
		FILE *file = fopen("m68kram_dump.txt", "w");
		if (file == NULL)
		{
			printf("Error opening file!\n");
			return;
		}

		fprintf(file, "\nDump of save data around\n");
		fprintf(file, "Offset   Address   Hex Bytes                                 		 ASCII              Decimal Values\n");

		for (u32 pos = 0; pos < MEM_SIZE; pos += 16)
		{
			s32 offset = (s32)pos;
			fprintf(file, "%+07d: %08X: ", offset, pos);

			for (int i = 0; i < 16; i++)
			{
				u8 byte = rdbyte(pos + i);
				fprintf(file, "%02X ", byte);
			}

			fprintf(file, " | ");

			for (int i = 0; i < 16; i++)
			{
				u8 byte = rdbyte(pos + i);
				fprintf(file, "%c", (byte >= 0x20 && byte <= 0x7E) ? byte : '.');
			}

			fprintf(file, " | ");

			for (int i = 0; i < 16; i++)
			{
				u8 byte = rdbyte(pos + i);
				fprintf(file, "%3d ", byte);
			}

			fprintf(file, "\n");
		}

		fprintf(file, "\n");
		fclose(file);

		printf("DONE!\n");
		dump_m68k_toggle = 0;
	}
}
```
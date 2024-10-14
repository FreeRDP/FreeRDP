/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Base64 Encoding & Decoding
 *
 * Copyright 2011-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <freerdp/config.h>

#include <winpr/crt.h>

#include <freerdp/crypto/crypto.h>

static const BYTE enc_base64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const BYTE enc_base64url[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static const signed char dec_base64url[] = {
	-1, /* 0        000	00	00000000	NUL	&#00;	 	Null character */
	-1, /* 1        001	01	00000001	SOH	&#01;	 	Start of Heading */
	-1, /* 2        002	02	00000010	STX	&#02;	 	Start of Text */
	-1, /* 3        003	03	00000011	ETX	&#03;	 	End of Text */
	-1, /* 4        004	04	00000100	EOT	&#04;	 	End of Transmission */
	-1, /* 5        005	05	00000101	ENQ	&#05;	 	Enquiry */
	-1, /* 6        006	06	00000110	ACK	&#06;	 	Acknowledge */
	-1, /* 7        007	07	00000111	BEL	&#07;	 	Bell, Alert */
	-1, /* 8        010	08	00001000	BS	&#08;	 	Backspace */
	-1, /* 9        011	09	00001001	HT	&#09;	 	Horizontal Tab */
	-1, /* 10        012	0A	00001010	LF	&#10;	 	Line Feed */
	-1, /* 11        013	0B	00001011	VT	&#11;	 	Vertical Tabulation */
	-1, /* 12        014	0C	00001100	FF	&#12;	 	Form Feed */
	-1, /* 13        015	0D	00001101	CR	&#13;	 	Carriage Return */
	-1, /* 14        016	0E	00001110	SO	&#14;	 	Shift Out */
	-1, /* 15        017	0F	00001111	SI	&#15;	 	Shift In */
	-1, /* 16        020	10	00010000	DLE	&#16;	 	Data Link Escape */
	-1, /* 17        021	11	00010001	DC1	&#17;	 	Device Control One (XON) */
	-1, /* 18        022	12	00010010	DC2	&#18;	 	Device Control Two */
	-1, /* 19        023	13	00010011	DC3	&#19;	 	Device Control Three (XOFF) */
	-1, /* 20        024	14	00010100	DC4	&#20;	 	Device Control Four */
	-1, /* 21        025	15	00010101	NAK	&#21;	 	Negative Acknowledge */
	-1, /* 22        026	16	00010110	SYN	&#22;	 	Synchronous Idle */
	-1, /* 23        027	17	00010111	ETB	&#23;	 	End of Transmission Block */
	-1, /* 24        030	18	00011000	CAN	&#24;	 	Cancel */
	-1, /* 25        031	19	00011001	EM	&#25;	 	End of medium */
	-1, /* 26        032	1A	00011010	SUB	&#26;	 	Substitute */
	-1, /* 27        033	1B	00011011	ESC	&#27;	 	Escape */
	-1, /* 28        034	1C	00011100	FS	&#28;	 	File Separator */
	-1, /* 29        035	1D	00011101	GS	&#29;	 	Group Separator */
	-1, /* 30        036	1E	00011110	RS	&#30;	 	Record Separator */
	-1, /* 31        037	1F	00011111	US	&#31;	 	Unit Separator */
	-1, /* 32        040	20	00100000	SP	&#32;	 	Space */
	-1, /* 33        041	21	00100001	!	&#33;	&excl;	Exclamation mark */
	-1, /* 34        042	22	00100010	"	&#34;	&quot;	Double quotes (or speech marks) */
	-1, /* 35        043	23	00100011	#	&#35;	&num;	Number sign */
	-1, /* 36        044	24	00100100	$	&#36;	&dollar;	Dollar */
	-1, /* 37        045	25	00100101	%	&#37;	&percnt;	Per cent sign */
	-1, /* 38        046	26	00100110	&	&#38;	&amp;	Ampersand */
	-1, /* 39        047	27	00100111	'	&#39;	&apos;	Single quote */
	-1, /* 40        050	28	00101000	(	&#40;	&lparen;	Open parenthesis (or open
	     * bracket)
	     */
	-1, /* 41        051	29	00101001	)	&#41;	&rparen;	Close parenthesis (or close
	     * bracket)
	     */
	-1, /* 42        052	2A	00101010	*	&#42;	&ast;	Asterisk */
	-1, /* 43        053	2B	00101011	+	&#43;	&plus;	Plus */
	-1, /* 44        054	2C	00101100	,	&#44;	&comma;	Comma */
	62, /* 45        055	2D	00101101	-	&#45;	 	Hyphen-minus */
	-1, /* 46        056	2E	00101110	.	&#46;	&period;	Period, dot or full stop */
	-1, /* 47        057	2F	00101111	/	&#47;	&sol;	Slash or divide */
	52, /* 48        060	30	00110000	0	&#48;	 	Zero */
	53, /* 49        061	31	00110001	1	&#49;	 	One */
	54, /* 50        062	32	00110010	2	&#50;	 	Two */
	55, /* 51        063	33	00110011	3	&#51;	 	Three */
	56, /* 52        064	34	00110100	4	&#52;	 	Four */
	57, /* 53        065	35	00110101	5	&#53;	 	Five */
	58, /* 54        066	36	00110110	6	&#54;	 	Six */
	59, /* 55        067	37	00110111	7	&#55;	 	Seven */
	60, /* 56        070	38	00111000	8	&#56;	 	Eight */
	61, /* 57        071	39	00111001	9	&#57;	 	Nine */
	-1, /* 58        072	3A	00111010	:	&#58;	&colon;	Colon */
	-1, /* 59        073	3B	00111011	;	&#59;	&semi;	Semicolon */
	-1, /* 60        074	3C	00111100	<	&#60;	&lt;	Less than (or open angled bracket)
	     */
	-1, /* 61        075	3D	00111101	=	&#61;	&equals;	Equals */
	-1, /* 62        076	3E	00111110	>	&#62;	&gt;	Greater than (or close angled
	     * bracket)
	     */
	-1, /* 63        077	3F	00111111	?	&#63;	&quest;	Question mark */
	-1, /* 64        100	40	01000000	@	&#64;	&commat;	At sign */
	0,  /* 65        101	41	01000001	A	&#65;	 	Uppercase A */
	1,  /* 66        102	42	01000010	B	&#66;	 	Uppercase B */
	2,  /* 67        103	43	01000011	C	&#67;	 	Uppercase C */
	3,  /* 68        104	44	01000100	D	&#68;	 	Uppercase D */
	4,  /* 69        105	45	01000101	E	&#69;	 	Uppercase E */
	5,  /* 70        106	46	01000110	F	&#70;	 	Uppercase F */
	6,  /* 71        107	47	01000111	G	&#71;	 	Uppercase G */
	7,  /* 72        110	48	01001000	H	&#72;	 	Uppercase H */
	8,  /* 73        111	49	01001001	I	&#73;	 	Uppercase I */
	9,  /* 74        112	4A	01001010	J	&#74;	 	Uppercase J */
	10, /* 75        113	4B	01001011	K	&#75;	 	Uppercase K */
	11, /* 76        114	4C	01001100	L	&#76;	 	Uppercase L */
	12, /* 77        115	4D	01001101	M	&#77;	 	Uppercase M */
	13, /* 78        116	4E	01001110	N	&#78;	 	Uppercase N */
	14, /* 79        117	4F	01001111	O	&#79;	 	Uppercase O */
	15, /* 80        120	50	01010000	P	&#80;	 	Uppercase P */
	16, /* 81        121	51	01010001	Q	&#81;	 	Uppercase Q */
	17, /* 82        122	52	01010010	R	&#82;	 	Uppercase R */
	18, /* 83        123	53	01010011	S	&#83;	 	Uppercase S */
	19, /* 84        124	54	01010100	T	&#84;	 	Uppercase T */
	20, /* 85        125	55	01010101	U	&#85;	 	Uppercase U */
	21, /* 86        126	56	01010110	V	&#86;	 	Uppercase V */
	22, /* 87        127	57	01010111	W	&#87;	 	Uppercase W */
	23, /* 88        130	58	01011000	X	&#88;	 	Uppercase X */
	24, /* 89        131	59	01011001	Y	&#89;	 	Uppercase Y */
	25, /* 90        132	5A	01011010	Z	&#90;	 	Uppercase Z */
	-1, /* 91        133	5B	01011011	[	&#91;	&lsqb;	Opening bracket */
	-1, /* 92        134	5C	01011100	\	&#92;	&bsol;	Backslash */
	-1, /* 93        135	5D	01011101	]	&#93;	&rsqb;	Closing bracket */
	-1, /* 94        136	5E	01011110	^	&#94;	&Hat;	Caret - circumflex */
	63, /* 95        137	5F	01011111	_	&#95;	&lowbar;	Underscore */
	-1, /* 96        140	60	01100000	`	&#96;	&grave;	Grave accent */
	26, /* 97        141	61	01100001	a	&#97;	 	Lowercase a */
	27, /* 98        142	62	01100010	b	&#98;	 	Lowercase b */
	28, /* 99        143	63	01100011	c	&#99;	 	Lowercase c */
	29, /* 100        144	64	01100100	d	&#100;	 	Lowercase d */
	30, /* 101        145	65	01100101	e	&#101;	 	Lowercase e */
	31, /* 102        146	66	01100110	f	&#102;	 	Lowercase f */
	32, /* 103        147	67	01100111	g	&#103;	 	Lowercase g */
	33, /* 104        150	68	01101000	h	&#104;	 	Lowercase h */
	34, /* 105        151	69	01101001	i	&#105;	 	Lowercase i */
	35, /* 106        152	6A	01101010	j	&#106;	 	Lowercase j */
	36, /* 107        153	6B	01101011	k	&#107;	 	Lowercase k */
	37, /* 108        154	6C	01101100	l	&#108;	 	Lowercase l */
	38, /* 109        155	6D	01101101	m	&#109;	 	Lowercase m */
	39, /* 110        156	6E	01101110	n	&#110;	 	Lowercase n */
	40, /* 111        157	6F	01101111	o	&#111;	 	Lowercase o */
	41, /* 112        160	70	01110000	p	&#112;	 	Lowercase p */
	42, /* 113        161	71	01110001	q	&#113;	 	Lowercase q */
	43, /* 114        162	72	01110010	r	&#114;	 	Lowercase r */
	44, /* 115        163	73	01110011	s	&#115;	 	Lowercase s */
	45, /* 116        164	74	01110100	t	&#116;	 	Lowercase t */
	46, /* 117        165	75	01110101	u	&#117;	 	Lowercase u */
	47, /* 118        166	76	01110110	v	&#118;	 	Lowercase v */
	48, /* 119        167	77	01110111	w	&#119;	 	Lowercase w */
	49, /* 120        170	78	01111000	x	&#120;	 	Lowercase x */
	50, /* 121        171	79	01111001	y	&#121;	 	Lowercase y */
	51, /* 122        172	7A	01111010	z	&#122;	 	Lowercase z */
	-1, /* 123        173	7B	01111011	{	&#123;	&lcub;	Opening brace */
	-1, /* 124        174	7C	01111100	|	&#124;	&verbar;	Vertical bar */
	-1, /* 125        175	7D	01111101	}	&#125;	&rcub;	Closing brace */
	-1, /* 126        176	7E	01111110	~	&#126;	&tilde;	Equivalency sign - tilde */
	-1, /* 127        177	7F	01111111	DEL	&#127;	 	Delete */
};
static const signed char dec_base64[] = {
	-1, /* 0        000	00	00000000	NUL	&#00;	 	Null character */
	-1, /* 1        001	01	00000001	SOH	&#01;	 	Start of Heading */
	-1, /* 2        002	02	00000010	STX	&#02;	 	Start of Text */
	-1, /* 3        003	03	00000011	ETX	&#03;	 	End of Text */
	-1, /* 4        004	04	00000100	EOT	&#04;	 	End of Transmission */
	-1, /* 5        005	05	00000101	ENQ	&#05;	 	Enquiry */
	-1, /* 6        006	06	00000110	ACK	&#06;	 	Acknowledge */
	-1, /* 7        007	07	00000111	BEL	&#07;	 	Bell, Alert */
	-1, /* 8        010	08	00001000	BS	&#08;	 	Backspace */
	-1, /* 9        011	09	00001001	HT	&#09;	 	Horizontal Tab */
	-1, /* 10        012	0A	00001010	LF	&#10;	 	Line Feed */
	-1, /* 11        013	0B	00001011	VT	&#11;	 	Vertical Tabulation */
	-1, /* 12        014	0C	00001100	FF	&#12;	 	Form Feed */
	-1, /* 13        015	0D	00001101	CR	&#13;	 	Carriage Return */
	-1, /* 14        016	0E	00001110	SO	&#14;	 	Shift Out */
	-1, /* 15        017	0F	00001111	SI	&#15;	 	Shift In */
	-1, /* 16        020	10	00010000	DLE	&#16;	 	Data Link Escape */
	-1, /* 17        021	11	00010001	DC1	&#17;	 	Device Control One (XON) */
	-1, /* 18        022	12	00010010	DC2	&#18;	 	Device Control Two */
	-1, /* 19        023	13	00010011	DC3	&#19;	 	Device Control Three (XOFF) */
	-1, /* 20        024	14	00010100	DC4	&#20;	 	Device Control Four */
	-1, /* 21        025	15	00010101	NAK	&#21;	 	Negative Acknowledge */
	-1, /* 22        026	16	00010110	SYN	&#22;	 	Synchronous Idle */
	-1, /* 23        027	17	00010111	ETB	&#23;	 	End of Transmission Block */
	-1, /* 24        030	18	00011000	CAN	&#24;	 	Cancel */
	-1, /* 25        031	19	00011001	EM	&#25;	 	End of medium */
	-1, /* 26        032	1A	00011010	SUB	&#26;	 	Substitute */
	-1, /* 27        033	1B	00011011	ESC	&#27;	 	Escape */
	-1, /* 28        034	1C	00011100	FS	&#28;	 	File Separator */
	-1, /* 29        035	1D	00011101	GS	&#29;	 	Group Separator */
	-1, /* 30        036	1E	00011110	RS	&#30;	 	Record Separator */
	-1, /* 31        037	1F	00011111	US	&#31;	 	Unit Separator */
	-1, /* 32        040	20	00100000	SP	&#32;	 	Space */
	-1, /* 33        041	21	00100001	!	&#33;	&excl;	Exclamation mark */
	-1, /* 34        042	22	00100010	"	&#34;	&quot;	Double quotes (or speech marks) */
	-1, /* 35        043	23	00100011	#	&#35;	&num;	Number sign */
	-1, /* 36        044	24	00100100	$	&#36;	&dollar;	Dollar */
	-1, /* 37        045	25	00100101	%	&#37;	&percnt;	Per cent sign */
	-1, /* 38        046	26	00100110	&	&#38;	&amp;	Ampersand */
	-1, /* 39        047	27	00100111	'	&#39;	&apos;	Single quote */
	-1, /* 40        050	28	00101000	(	&#40;	&lparen;	Open parenthesis (or open
	     * bracket)
	     */
	-1, /* 41        051	29	00101001	)	&#41;	&rparen;	Close parenthesis (or close
	     * bracket)
	     */
	-1, /* 42        052	2A	00101010	*	&#42;	&ast;	Asterisk */
	62, /* 43        053	2B	00101011	+	&#43;	&plus;	Plus */
	-1, /* 44        054	2C	00101100	,	&#44;	&comma;	Comma */
	-1, /* 45        055	2D	00101101	-	&#45;	 	Hyphen-minus */
	-1, /* 46        056	2E	00101110	.	&#46;	&period;	Period, dot or full stop */
	63, /* 47        057	2F	00101111	/	&#47;	&sol;	Slash or divide */
	52, /* 48        060	30	00110000	0	&#48;	 	Zero */
	53, /* 49        061	31	00110001	1	&#49;	 	One */
	54, /* 50        062	32	00110010	2	&#50;	 	Two */
	55, /* 51        063	33	00110011	3	&#51;	 	Three */
	56, /* 52        064	34	00110100	4	&#52;	 	Four */
	57, /* 53        065	35	00110101	5	&#53;	 	Five */
	58, /* 54        066	36	00110110	6	&#54;	 	Six */
	59, /* 55        067	37	00110111	7	&#55;	 	Seven */
	60, /* 56        070	38	00111000	8	&#56;	 	Eight */
	61, /* 57        071	39	00111001	9	&#57;	 	Nine */
	-1, /* 58        072	3A	00111010	:	&#58;	&colon;	Colon */
	-1, /* 59        073	3B	00111011	;	&#59;	&semi;	Semicolon */
	-1, /* 60        074	3C	00111100	<	&#60;	&lt;	Less than (or open angled bracket)
	     */
	-1, /* 61        075	3D	00111101	=	&#61;	&equals;	Equals */
	-1, /* 62        076	3E	00111110	>	&#62;	&gt;	Greater than (or close angled
	     * bracket)
	     */
	-1, /* 63        077	3F	00111111	?	&#63;	&quest;	Question mark */
	-1, /* 64        100	40	01000000	@	&#64;	&commat;	At sign */
	0,  /* 65        101	41	01000001	A	&#65;	 	Uppercase A */
	1,  /* 66        102	42	01000010	B	&#66;	 	Uppercase B */
	2,  /* 67        103	43	01000011	C	&#67;	 	Uppercase C */
	3,  /* 68        104	44	01000100	D	&#68;	 	Uppercase D */
	4,  /* 69        105	45	01000101	E	&#69;	 	Uppercase E */
	5,  /* 70        106	46	01000110	F	&#70;	 	Uppercase F */
	6,  /* 71        107	47	01000111	G	&#71;	 	Uppercase G */
	7,  /* 72        110	48	01001000	H	&#72;	 	Uppercase H */
	8,  /* 73        111	49	01001001	I	&#73;	 	Uppercase I */
	9,  /* 74        112	4A	01001010	J	&#74;	 	Uppercase J */
	10, /* 75        113	4B	01001011	K	&#75;	 	Uppercase K */
	11, /* 76        114	4C	01001100	L	&#76;	 	Uppercase L */
	12, /* 77        115	4D	01001101	M	&#77;	 	Uppercase M */
	13, /* 78        116	4E	01001110	N	&#78;	 	Uppercase N */
	14, /* 79        117	4F	01001111	O	&#79;	 	Uppercase O */
	15, /* 80        120	50	01010000	P	&#80;	 	Uppercase P */
	16, /* 81        121	51	01010001	Q	&#81;	 	Uppercase Q */
	17, /* 82        122	52	01010010	R	&#82;	 	Uppercase R */
	18, /* 83        123	53	01010011	S	&#83;	 	Uppercase S */
	19, /* 84        124	54	01010100	T	&#84;	 	Uppercase T */
	20, /* 85        125	55	01010101	U	&#85;	 	Uppercase U */
	21, /* 86        126	56	01010110	V	&#86;	 	Uppercase V */
	22, /* 87        127	57	01010111	W	&#87;	 	Uppercase W */
	23, /* 88        130	58	01011000	X	&#88;	 	Uppercase X */
	24, /* 89        131	59	01011001	Y	&#89;	 	Uppercase Y */
	25, /* 90        132	5A	01011010	Z	&#90;	 	Uppercase Z */
	-1, /* 91        133	5B	01011011	[	&#91;	&lsqb;	Opening bracket */
	-1, /* 92        134	5C	01011100	\	&#92;	&bsol;	Backslash */
	-1, /* 93        135	5D	01011101	]	&#93;	&rsqb;	Closing bracket */
	-1, /* 94        136	5E	01011110	^	&#94;	&Hat;	Caret - circumflex */
	-1, /* 95        137	5F	01011111	_	&#95;	&lowbar;	Underscore */
	-1, /* 96        140	60	01100000	`	&#96;	&grave;	Grave accent */
	26, /* 97        141	61	01100001	a	&#97;	 	Lowercase a */
	27, /* 98        142	62	01100010	b	&#98;	 	Lowercase b */
	28, /* 99        143	63	01100011	c	&#99;	 	Lowercase c */
	29, /* 100        144	64	01100100	d	&#100;	 	Lowercase d */
	30, /* 101        145	65	01100101	e	&#101;	 	Lowercase e */
	31, /* 102        146	66	01100110	f	&#102;	 	Lowercase f */
	32, /* 103        147	67	01100111	g	&#103;	 	Lowercase g */
	33, /* 104        150	68	01101000	h	&#104;	 	Lowercase h */
	34, /* 105        151	69	01101001	i	&#105;	 	Lowercase i */
	35, /* 106        152	6A	01101010	j	&#106;	 	Lowercase j */
	36, /* 107        153	6B	01101011	k	&#107;	 	Lowercase k */
	37, /* 108        154	6C	01101100	l	&#108;	 	Lowercase l */
	38, /* 109        155	6D	01101101	m	&#109;	 	Lowercase m */
	39, /* 110        156	6E	01101110	n	&#110;	 	Lowercase n */
	40, /* 111        157	6F	01101111	o	&#111;	 	Lowercase o */
	41, /* 112        160	70	01110000	p	&#112;	 	Lowercase p */
	42, /* 113        161	71	01110001	q	&#113;	 	Lowercase q */
	43, /* 114        162	72	01110010	r	&#114;	 	Lowercase r */
	44, /* 115        163	73	01110011	s	&#115;	 	Lowercase s */
	45, /* 116        164	74	01110100	t	&#116;	 	Lowercase t */
	46, /* 117        165	75	01110101	u	&#117;	 	Lowercase u */
	47, /* 118        166	76	01110110	v	&#118;	 	Lowercase v */
	48, /* 119        167	77	01110111	w	&#119;	 	Lowercase w */
	49, /* 120        170	78	01111000	x	&#120;	 	Lowercase x */
	50, /* 121        171	79	01111001	y	&#121;	 	Lowercase y */
	51, /* 122        172	7A	01111010	z	&#122;	 	Lowercase z */
	-1, /* 123        173	7B	01111011	{	&#123;	&lcub;	Opening brace */
	-1, /* 124        174	7C	01111100	|	&#124;	&verbar;	Vertical bar */
	-1, /* 125        175	7D	01111101	}	&#125;	&rcub;	Closing brace */
	-1, /* 126        176	7E	01111110	~	&#126;	&tilde;	Equivalency sign - tilde */
	-1, /* 127        177	7F	01111111	DEL	&#127;	 	Delete */
};

static INLINE char* base64_encode_ex(const BYTE* WINPR_RESTRICT alphabet,
                                     const BYTE* WINPR_RESTRICT data, size_t length, BOOL pad,
                                     BOOL crLf, size_t lineSize)
{
	int c = 0;
	size_t blocks = 0;
	size_t outLen = (length + 3) * 4 / 3;
	size_t extra = 0;
	if (crLf)
	{
		size_t nCrLf = (outLen + lineSize - 1) / lineSize;
		extra = nCrLf * 2;
	}
	size_t outCounter = 0;

	const BYTE* q = data;
	BYTE* p = malloc(outLen + extra + 1ull);
	if (!p)
		return NULL;

	char* ret = (char*)p;

	/* b1, b2, b3 are input bytes
	 *
	 * 0         1         2
	 * 012345678901234567890123
	 * |  b1  |  b2   |  b3   |
	 *
	 * [ c1 ]     [  c3 ]
	 *      [  c2 ]     [  c4 ]
	 *
	 * c1, c2, c3, c4 are output chars in base64
	 */

	/* first treat complete blocks */
	blocks = length - (length % 3);
	for (size_t i = 0; i < blocks; i += 3, q += 3)
	{
		c = (q[0] << 16) + (q[1] << 8) + q[2];

		*p++ = alphabet[(c & 0x00FC0000) >> 18];
		*p++ = alphabet[(c & 0x0003F000) >> 12];
		*p++ = alphabet[(c & 0x00000FC0) >> 6];
		*p++ = alphabet[c & 0x0000003F];

		outCounter += 4;
		if (crLf && (outCounter % lineSize == 0))
		{
			*p++ = '\r';
			*p++ = '\n';
		}
	}

	/* then remainder */
	switch (length % 3)
	{
		case 0:
			break;
		case 1:
			c = (q[0] << 16);
			*p++ = alphabet[(c & 0x00FC0000) >> 18];
			*p++ = alphabet[(c & 0x0003F000) >> 12];
			if (pad)
			{
				*p++ = '=';
				*p++ = '=';
			}
			break;
		case 2:
			c = (q[0] << 16) + (q[1] << 8);
			*p++ = alphabet[(c & 0x00FC0000) >> 18];
			*p++ = alphabet[(c & 0x0003F000) >> 12];
			*p++ = alphabet[(c & 0x00000FC0) >> 6];
			if (pad)
				*p++ = '=';
			break;
	}

	if (crLf && length % 3)
	{
		*p++ = '\r';
		*p++ = '\n';
	}
	*p = 0;

	return ret;
}

static INLINE char* base64_encode(const BYTE* WINPR_RESTRICT alphabet,
                                  const BYTE* WINPR_RESTRICT data, size_t length, BOOL pad)
{
	return base64_encode_ex(alphabet, data, length, pad, FALSE, 64);
}

static INLINE int base64_decode_char(const signed char* WINPR_RESTRICT alphabet, char c)
{
	if (c <= '\0')
		return -1;

	return alphabet[(size_t)c];
}

static INLINE void* base64_decode(const signed char* WINPR_RESTRICT alphabet,
                                  const char* WINPR_RESTRICT s, size_t length,
                                  size_t* WINPR_RESTRICT data_len, BOOL pad)
{
	int n[4];
	BYTE* q = NULL;
	BYTE* data = NULL;
	size_t nBlocks = 0;
	size_t outputLen = 0;
	const size_t remainder = length % 4;

	if ((pad && remainder > 0) || (remainder == 1))
		return NULL;

	if (!pad && remainder)
		length += 4 - remainder;

	q = data = (BYTE*)malloc(length / 4 * 3 + 1);
	if (!q)
		return NULL;

	/* first treat complete blocks */
	nBlocks = (length / 4);
	outputLen = 0;

	if (nBlocks < 1)
	{
		free(data);
		return NULL;
	}

	for (size_t i = 0; i < nBlocks - 1; i++, q += 3)
	{
		n[0] = base64_decode_char(alphabet, *s++);
		n[1] = base64_decode_char(alphabet, *s++);
		n[2] = base64_decode_char(alphabet, *s++);
		n[3] = base64_decode_char(alphabet, *s++);

		if ((n[0] == -1) || (n[1] == -1) || (n[2] == -1) || (n[3] == -1))
			goto out_free;

		q[0] = (n[0] << 2) + (n[1] >> 4);
		q[1] = ((n[1] & 15) << 4) + (n[2] >> 2);
		q[2] = ((n[2] & 3) << 6) + n[3];
		outputLen += 3;
	}

	/* treat last block */
	n[0] = base64_decode_char(alphabet, *s++);
	n[1] = base64_decode_char(alphabet, *s++);
	if ((n[0] == -1) || (n[1] == -1))
		goto out_free;

	n[2] = remainder == 2 ? -1 : base64_decode_char(alphabet, *s++);
	n[3] = remainder >= 2 ? -1 : base64_decode_char(alphabet, *s++);

	q[0] = (n[0] << 2) + (n[1] >> 4);
	if (n[2] == -1)
	{
		/* XX== */
		outputLen += 1;
		if (n[3] != -1)
			goto out_free;

		q[1] = ((n[1] & 15) << 4);
	}
	else if (n[3] == -1)
	{
		/* yyy= */
		outputLen += 2;
		q[1] = ((n[1] & 15) << 4) + (n[2] >> 2);
		q[2] = ((n[2] & 3) << 6);
	}
	else
	{
		/* XXXX */
		outputLen += 3;
		q[0] = (n[0] << 2) + (n[1] >> 4);
		q[1] = ((n[1] & 15) << 4) + (n[2] >> 2);
		q[2] = ((n[2] & 3) << 6) + n[3];
	}

	if (data_len)
		*data_len = outputLen;
	data[outputLen] = '\0';

	return data;
out_free:
	free(data);
	return NULL;
}

char* crypto_base64_encode_ex(const BYTE* WINPR_RESTRICT data, size_t length, BOOL withCrLf)
{
	return base64_encode_ex(enc_base64, data, length, TRUE, withCrLf, 64);
}

char* crypto_base64_encode(const BYTE* WINPR_RESTRICT data, size_t length)
{
	return base64_encode(enc_base64, data, length, TRUE);
}

void crypto_base64_decode(const char* WINPR_RESTRICT enc_data, size_t length,
                          BYTE** WINPR_RESTRICT dec_data, size_t* WINPR_RESTRICT res_length)
{
	*dec_data = base64_decode(dec_base64, enc_data, length, res_length, TRUE);
}

char* crypto_base64url_encode(const BYTE* WINPR_RESTRICT data, size_t length)
{
	return base64_encode(enc_base64url, data, length, FALSE);
}

void crypto_base64url_decode(const char* WINPR_RESTRICT enc_data, size_t length,
                             BYTE** WINPR_RESTRICT dec_data, size_t* WINPR_RESTRICT res_length)
{
	*dec_data = base64_decode(dec_base64url, enc_data, length, res_length, FALSE);
}

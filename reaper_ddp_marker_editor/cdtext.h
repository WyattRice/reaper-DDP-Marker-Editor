#ifndef CDTEXT_HEADER_INCLUDED
#define CDTEXT_HEADER_INCLUDED

#define CDTEXT_TITLE			0x00
#define CDTEXT_PERFORMER		0x01
#define CDTEXT_SONGWRITER		0x02
#define CDTEXT_COMPOSER			0x03
#define CDTEXT_ARRANGER			0x04
#define CDTEXT_MESSAGE			0x05
#define CDTEXT_IDENTIFICATION	0x06
#define CDTEXT_GENRE			0x07
#define CDTEXT_TABLE_OF_CONTENT 0x08
#define CDTEXT_SECOND_TOC		0x09
#define CDTEXT_RESERVED1  		0x0A
#define CDTEXT_RESERVED2  		0x0B
#define CDTEXT_RESERVED3  		0x0C
#define CDTEXT_CLOSED_INFO		0x0D
#define CDTEXT_EAN_ISRC			0x0E
#define CDTEXT_SIZE_INFORMATION 0x0F

#define MAX_CDTEXT				(CDTEXT_SIZE_INFORMATION + 1)
#define MAX_ALBUM_CDTEXT		(CDTEXT_GENRE + 1)
#define MAX_TRACK_CDTEXT		(CDTEXT_MESSAGE + 1)

#define NUM_CDTEXT_GENRES		29
#define NUM_CDTEXT_LANGUAGES	128


typedef struct {
	unsigned char type[4];
	char text[12];
	unsigned char crc[2];
} CDTEXTPackData;

char *CDTEXTKeyList[MAX_ALBUM_CDTEXT] = { // represent CDTEXT_* constants
	"TITLE",
	"PERFORMER",
	"SONGWRITER",
	"COMPOSER",
	"ARRANGER",
	"MESSAGE",
	"IDENTIFICATION",
	"GENRE",
};


char *CDTEXTGenreList[NUM_CDTEXT_GENRES + 1] = { // index of the text is a genre code
	"", // unused
	"",
	"Adult Contemporary",
	"Alternative Rock",
	"Bluegrass",
	"Childrens Music",
	"Classical",
	"Contemporary Christian",
	"Country",
	"Dance",
	"Easy Listening",
	"Erotic",
	"Folk",
	"Gospel",
	"Hip Hop",
	"Jazz",
	"Latin",
	"Musical",
	"New Age",
	"Opera",
	"Operetta",
	"Pop",
	"Rap",
	"Reggae",
	"Rock Music",
	"Rhythm & Blues",
	"Sound Effects",
	"Soundtrack",
	"Spoken Word",
	"World Music",
};


char *CDTEXTLanguageList[NUM_CDTEXT_LANGUAGES] = { // index of the text is a language code
	"",
	"Albanian",
	"Breton",
	"Catalan",
	"Croatian",
	"Welsh",
	"Czech",
	"Danish",
	"German",
	"English",
	"Spanish",
	"Esperanto",
	"Estonian",
	"Basque",
	"Faroese",
	"French",
	"Frisian",
	"Irish",
	"Gaelic",
	"Galician",
	"Icelandic",
	"Italian",
	"Lappish",
	"Latin",
	"Latvian",
	"Luxembourgian",
	"Lithuanian",
	"Hungarian",
	"Maltese",
	"Dutch",
	"Norwegian",
	"Occitan",
	"Polish",
	"Portugese",
	"Romanian",
	"Romansh",
	"Serbian",
	"Slovak",
	"Slovenian",
	"Finnish",
	"Swedish",
	"Turkish",
	"Flemish",
	"Wallon",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"Zulu",
	"Vietnamese",
	"Uzbek",
	"Urdu",
	"Ukrainian",
	"Thai",
	"Telugu",
	"Tatar",
	"Tamil",
	"Tadzhik",
	"Swahili",
	"SrananTongo",
	"Somali",
	"Sinhalese",
	"Shona",
	"Serbo-croat",
	"Ruthenian",
	"Russian",
	"Quechua",
	"Pushtu",
	"Punjabi",
	"Persian",
	"Papamiento",
	"Oriya",
	"Nepali",
	"Ndebele",
	"Marathi",
	"Moldavian",
	"Malaysian",
	"Malagasay",
	"Macedonian",
	"Laotian",
	"Korean",
	"Khmer",
	"Kazakh",
	"Kannada",
	"Japanese",
	"Indonesian",
	"Hindi",
	"Hebrew",
	"Hausa",
	"Gurani",
	"Gujurati",
	"Greek",
	"Georgian",
	"Fulani",
	"Dari",
	"Churash",
	"Chinese",
	"Burmese",
	"Bulgarian",
	"Bengali",
	"Bielorussian",
	"Bambora",
	"Azerbaijani",
	"Assamese",
	"Armenian",
	"Arabic",
	"Amharic",
};

extern char *CDTEXTKeyList[MAX_ALBUM_CDTEXT]; // represent CDTEXT_* constants
extern char *CDTEXTGenreList[NUM_CDTEXT_GENRES + 1]; // index of the text is a genre code
extern char *CDTEXTLanguageList[NUM_CDTEXT_LANGUAGES]; // index of the text is a language code

#endif

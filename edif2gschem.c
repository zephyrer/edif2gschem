/* edif2gschem -- EDIF to gEDA gschem converter */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <math.h>

#define global

#include "ed.h"
#include "eelibsl.h"

#include "libgeda/defines.h"
#include "libgeda/colors.h"

/* Debug level */
int bug = 0;
int yydebug = 0;

const char GEDA_VERSION[] = "20121203";
const char GEDA_FILE_VERSION[] ="2";

enum {TEXT_NO_VISIBLE = 1};
enum {FILE_NAME_LIM = 128};
int magn = 1;
int shift = 10000;
int font_denom = 13;

/* Used in edif.y */
struct con *cons = NULL, *cptr = NULL;
struct inst *insts = NULL, *iptr = NULL;
float scale;

char *InFile = "-";
char FileNameKiPro[FILE_NAME_LIM];
char FileNameEESchema[FILE_NAME_LIM];
char FileNameLib[FILE_NAME_LIM];

FILE *FileLib = NULL, *FileKiPro = NULL;

extern int nPages;

int lib_create(const char *dir_name, LibraryStruct *lib);

int mkdir_or_exist(const char *dir_name)
{
	if (mkdir(dir_name, 0777)) {
		if (errno != EEXIST) {
			fprintf(stderr, "can't create '%s' dir\n", dir_name);
			return 0;
		}
	}

	return 1;
}

char *pin_name_format(char *str);

int main(int argc, char *argv[])
{
	FILE *fd_in;
	char *prog_name;
	int err = 0;
	const char lib_dir_name_base[] = "sym";
	char *lib_dir_name;
	LibraryStruct *lib_p;
	int i;

#if 0
printf("%s\n", pin_name_format("r\\e\\se\\t"));
printf("%s\n", pin_name_format("r\\e\\set"));
printf("%s\n", pin_name_format("r\\e\\set\\"));
printf("%s\n", pin_name_format("\\reset\\\\ab\\"));
return 0;
#endif

	prog_name = strrchr(argv[0], '/');
	if (prog_name)
		prog_name++;
	else
		prog_name = argv[0];

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <EDIF-file>\n", prog_name);
		exit(1);
	}

	InFile = argv[1];
	fd_in = fopen(InFile, "r");
	if (fd_in == NULL) {
		fprintf(stderr, "can't open '%s' file\n", InFile);
		exit(2);
	}

	Libs = NULL;
	strcpy(fName, "");
	err = ParseEDIF(fd_in, stderr);
	if (err) {
		fprintf(stderr, "EDIF file parse error\n");
		fclose(fd_in);
		exit(1);
	}

	if (!mkdir_or_exist(lib_dir_name_base)) {
		fclose(fd_in);
		exit(2);
	}

	i = sizeof(char)*(strlen(lib_dir_name_base) +
				1 + strlen(fName) + 1);
	lib_dir_name = malloc(i);
	snprintf(lib_dir_name, i, "%s/%s", lib_dir_name_base, fName);

	if (!mkdir_or_exist(lib_dir_name)) {
		fclose(fd_in);
		exit(2);
	}

#if 0
	fd_lib = fopen(FileNameLib, "wt");
	if (fd_lib == NULL ) {
		fprintf(stderr, "can't create '%s'\n", FileNameLib);
		exit(2);
	}
#endif

	lib_p = Libs;
	while (lib_p!= NULL) {
		if (lib_p->isSheet) {
			/* Create schematic file */
			fprintf(stderr, "Sheet '%s'\n", lib_p->Name);
			printf("Page '%s': %d\n", lib_p->Name, lib_p->NumOfParts);
		} else {
			/* Create library dirs with symbols */
			if (lib_p->NumOfParts)
				if (!lib_create(lib_dir_name, lib_p)) {
					fclose(fd_in);
					exit(2);
				}
		}
		lib_p = lib_p->nxt;
	}

	fprintf(stderr, "Total pages: %d\n", nPages);
	fclose(fd_in);
	exit(err);
}

typedef enum {NORMAL_PIN, BUS_PIN} PIN_TYPE;
typedef enum {LEFT_PIN, RIGHT_PIN} PIN_WHICHEND;

const char *field_id_str(int field_id)
{
	static const char *field_id_str[] = {
		"refdes",
		"value",
		"field1",
		"field2",
		"field3",
		"field4",
		"field5",
		"field6",
		"field7",
		"field8",
		"module_pcb",
		"sheet_name",
	};

	if (field_id < sizeof(field_id_str)/sizeof(typeof(field_id_str[0]))) {
		return field_id_str[field_id];
	}

	fprintf(stderr, "WARN: unknown field_id %d\n", field_id);
	return "unknown_field";
}

const char *pintype_str(int pintype)
{
	static const char *pintype_str[] = {
		"in",
		"out",
		"io",
		"tri",
		"pas",
		"unknown",
		"pwr",
		"oc",
		"oe",
	};

	if (pintype < sizeof(pintype_str)/sizeof(typeof(pintype_str[0]))) {
		return pintype_str[pintype];
	}

	fprintf(stderr, "WARN: unknown pintype %d\n", pintype);
	return "unknown";
}

/* Return reformatted pin name string or NULL, returned string  must be freed
 * with free(). */
char *pin_name_format(char *str)
{
	unsigned int n;
	char *p, *p_prev, *p_new;
printf("DBG: %s  ", str);

	/* If pin name is R\E\SET replace it with \_RE\_SET */
	p = strstr(str, "\\");
	if (p == NULL) {
		return p;
	}

	n = 1;
	do {
		p_prev = p;
		p = strstr(p_prev, "\\");
		if ((p - p_prev) > 1)
			n++;
	} while (p++ != NULL);

	p_new = malloc((strlen(str) + 2*n + 1)*sizeof(char));
	if (p_new == NULL) {
		fprintf(stderr, "can't malloc() in %s()\n", __func__);
		return p_new;
	}

#if 0
	if (*str == '\\')
		str++;
#endif
	n = 0;
	p = p_new;
	p_prev = str;
	while (*str != '\0') {
		if (*(str + 1) != '\\') {
			if (*str != '\\')
				*p++ = *str;
			str++;
			continue;
		}

		if ((str - p_prev) > 1 || n == 0) {
			n++;
			strcpy(p, "\\_");
			p += 2;
		}
		*p++ = *str++;
		p_prev = str++;
	}

	*p = '\0';
	return p_new;
}

int lib_entry_create(const char *dir_name, LibraryEntryStruct *lib_en)
{
	char *file_name;
	FILE *fd;
	struct stat st;
	LibraryFieldEntry *field;
	LibraryDrawEntryStruct *drawing;
	char *str;
	int pinseq, x1, y1, size;
	int i;

	i = sizeof(char)*(strlen(dir_name) + 1 + strlen(lib_en->Name) + 4 + 1);
	file_name = malloc(i);
	snprintf(file_name, i, "%s/%s.sym", dir_name, lib_en->Name);

	if (0 == stat(file_name, &st)) {
		fprintf(stderr, "lib entry file exist '%s'\n", file_name);
		free(file_name);
		return 0;
	} else if (errno != ENOENT) {
		fprintf(stderr, "can't stat() lib entry file '%s', errno: %d\n",
				file_name, errno);
		free(file_name);
		return 0;
	}

	fd = fopen(file_name, "w");
	if (fd == NULL) {
		fprintf(stderr, "can't create lib entry file '%s'\n", file_name);
		return 0;
	}

	free(file_name);

	fprintf(fd, "v %s %s\n", GEDA_VERSION, GEDA_FILE_VERSION);

	/* Symbol attributes */
	x1 = shift + magn*lib_en->PrefixPosX;
	y1 = shift + magn*lib_en->PrefixPosY;
	size = lib_en->PrefixSize/font_denom;
	fprintf(fd, "T %d %d %d %d %d %d %d %d %d\n",
			x1, y1,				/* x y */
			ATTRIBUTE_COLOR,		/* color */
			size,				/* size (points) */
			lib_en->DrawPrefix, 1,	/* visibility show_name_value */
			0, LOWER_LEFT, 1);	/* angle alignment num_lines */
	fprintf(fd, "refdes=%s\n", lib_en->Prefix);

	x1 = shift + magn*lib_en->NamePosX;
	y1 = shift + magn*lib_en->NamePosY;
	size = lib_en->NameSize/font_denom;
	fprintf(fd, "T %d %d %d %d %d %d %d %d %d\n",
			x1, y1,				/* x y */
			ATTRIBUTE_COLOR,		/* color */
			size,				/* size (points) */
			lib_en->DrawName, 1,	/* visibility show_name_value */
			0, LOWER_LEFT, 1);	/* angle alignment num_lines */
	fprintf(fd, "device=%s\n", lib_en->Name);

fprintf(stderr, "WARN: skipping AliasList '%s'\n", lib_en->AliasList);
fprintf(stderr, "WARN: skipping NameOrient %d, PrefixOrient %d\n",
		lib_en->NameOrient, lib_en->PrefixOrient);
fprintf(stderr, "WARN: skipping NumOfUnits %d\n", lib_en->NumOfUnits);
fprintf(stderr, "WARN: skipping TextInside %d\n", lib_en->TextInside);

	for (field = lib_en->Fields; field != NULL; field = field->nxt) {
		if ((field->Text == NULL) || (strlen(field->Text) == 0))
			continue;

		x1 = shift + magn*field->PosX;
		y1 = shift + magn*field->PosY;
		size = lib_en->NameSize/font_denom;
		fprintf(fd, "T %d %d %d %d %d %d %d %d %d\n",
				x1, y1,			/* x y */
				ATTRIBUTE_COLOR,	/* color */
				field->Size,		/* size (points) */
				!field->Flags & TEXT_NO_VISIBLE, 0,
						/* visibility show_name_value */
				0, LOWER_LEFT, 1);
						/* angle alignment num_lines */
		fprintf(fd, "%s=%s\n",
				field_id_str(field->FieldId), field->Text);

fprintf(stderr, "WARN: skipping Orient %d\n", field->Orient);
	}

	pinseq = 1;
	for (drawing = lib_en->Drawings;
				drawing != NULL; drawing = drawing->nxt) {
		switch (drawing->DrawType) {
		case CIRCLE_DRAW_TYPE: {
			LibraryDrawCircle *circle = &(drawing->U.Circ);
			int x1, y1;

			x1 = shift + magn*circle->x;
			y1 = shift + magn*circle->y;
			fprintf(fd, "V %d %d %d %d %d %d %d "
						"%d %d %d %d %d %d %d %d\n",
							/* x y radius */
					x1, y1, circle->r/2,
							/* color width */
					GRAPHIC_COLOR, circle->width,
					1,	/* capstyle */
					/* dashstyle dashlength dashspace */
					0, -1, -1,
					0, -1,	/* filltype fillwidth */
					/* angle1 pitch1 angle2 pitch2 */
					-1, -1, -1, -1);
			break;
		}
		case PIN_DRAW_TYPE: {
			LibraryDrawPin *pin = &(drawing->U.Pin);
			int number_align, label_align, angle = 0;
			int x1 = shift, x2 = shift, y1 = shift, y2 = shift;

			switch (pin->Orient) {
			case 'R':
				x1 += magn*pin->posX;
				y1 += magn*pin->posY;
				x2 = x1 + magn*pin->Len;
				y2 = y1;
				number_align = LOWER_LEFT;
				label_align = MIDDLE_LEFT;
				break;
			case 'U':
				x1 += magn*pin->posX;
				y1 += magn*pin->posY;
				x2 = x1;
				y2 = y1 + magn*pin->Len;
				angle = 90;
				number_align = LOWER_LEFT;
				label_align = MIDDLE_LEFT;
				break;
			case 'D':
				x1 += magn*pin->posX;
				y1 += magn*pin->posY;
				x2 = x1;
				y2 = y1 - magn*pin->Len;
				angle = 90;
				number_align = LOWER_RIGHT;
				label_align = MIDDLE_RIGHT;
				break;
			default:
				fprintf(stderr,
					"WARN: skipping pin orientation %d for %s\n",
						pin->Orient, pin->Name);
				/* no break */
			case 0:
			case 'L':
				x1 += magn*pin->posX;
				y1 += magn*pin->posY;
				x2 = x1 - magn*pin->Len;;
				y2 = y1;
				number_align = LOWER_RIGHT;
				label_align = MIDDLE_RIGHT;
				break;
			}

			fprintf(fd, "P %d %d %d %d %d %d %d\n",
					x1, y1, x2, y2,	/* x1 y1 x2 y2 */
					PIN_COLOR,		/* color */
					NORMAL_PIN,		/* pintype */
					LEFT_PIN);		/* whichend */
			fprintf(fd, "{\n");

/* TODO: if pin is not visible, make it zero length and hide attributes */

			/* Pin attributes */
			size = pin->SizeNum/font_denom;
			fprintf(fd, "T %d %d %d %d %d %d %d %d %d\n",
					x1, y1,			/* x y */
					ATTRIBUTE_COLOR,	/* color */
					size,		/* size (points) */
						/* visibility show_name_value */
					lib_en->DrawPinNum, 1,
						/* angle alignment num_lines */
					angle, number_align, 1);
			fprintf(fd, "pinnumber=%s\n", pin->Num);

			fprintf(fd, "T %d %d %d %d %d %d %d %d %d\n",
					x1, y1,			/* x y */
					ATTRIBUTE_COLOR,	/* color */
					4,		/* size (points) */
					0, 0,	/* visibility show_name_value */
						/* angle alignment num_lines */
					angle, MIDDLE_MIDDLE, 1);
			fprintf(fd, "pinsec=%d\n", pinseq++);

			size = pin->SizeName/font_denom;
			fprintf(fd, "T %d %d %d %d %d %d %d %d %d\n",
					x2, y2,			/* x y */
					ATTRIBUTE_COLOR,	/* color */
					size,		/* size (points) */
						/* visibility show_name_value */
					lib_en->DrawPinName, 1,
						/* angle alignment num_lines */
					angle, label_align, 1);
			str = pin_name_format((pin->ReName == NULL)?
						pin->Name: pin->ReName);
			if (str == NULL) {
				fprintf(fd, "pinlabel=%s\n",
						(pin->ReName == NULL)?
							pin->Name: pin->ReName);
			} else {
				fprintf(fd, "pinlabel=%s\n", str);
				free(str);
			}

			fprintf(fd, "T %d %d %d %d %d %d %d %d %d\n",
					x1, y1,			/* x y */
					ATTRIBUTE_COLOR,	/* color */
					4,	/* size (points) */
					0, 0,	/* visibility show_name_value */
						/* angle alignment num_lines */
					angle, MIDDLE_MIDDLE, 1);
			fprintf(fd, "pintype=%s\n", pintype_str(pin->PinType));

/* TODO: not all pintypes are parsed in edif.y */
#if 0
printf("DBG: pinttype '%c' pinname '%s'\n", pin->PinType, pin->Name);
#endif

			fprintf(fd, "}\n");

			break;
		}
		case POLYLINE_DRAW_TYPE: {
			LibraryDrawPolyline *poly = &(drawing->U.Poly);
			int *coords, x1, x2, y1, y2;
			int i;

/* TODO: draw as polyline in file format version 2? */

			if (poly->n < 2) {
				fprintf(stderr,
					"WARN: skipping polyline with %d points\n",
					poly->n);
				break;
			}

			coords = poly->PolyList;

			x1 = shift + magn*coords[0];
			y1 = shift + magn*coords[1];
			coords += 2;

			for (i = 1; i < poly->n; i++) {
				x2 = shift + magn*coords[0];
				y2 = shift + magn*coords[1];
				coords += 2;
				fprintf(fd, "L %d %d %d %d %d %d %d %d %d %d\n",
					x1, y1, x2, y2,	/* x1 y1 x2 y2 */
							/* color width */
					GRAPHIC_COLOR, poly->width,
					1, 0,	/* capstyle dashstyle */
						/* dashlength dashspace */
					-1, -1);
				x1 = x2;
				y1 = y2;
			}

			if (poly->Fill) {
				fprintf(stderr,
					"WARN: ignoring polyline fill type %d\n",
						poly->Fill);
			}

			break;
		}
		case SQUARE_DRAW_TYPE: {
			LibraryDrawSquare *box = &(drawing->U.Sqr);
			int x1, x2, y1, y2;

			x1 = shift + magn*box->x1;
			y1 = shift + magn*box->y1;
			x2 = shift + magn*box->x2;
			y2 = shift + magn*box->y2;
			fprintf(fd, "B %d %d %d %d %d %d %d %d %d %d %d %d "
						"%d %d %d %d\n",
				min(x1, x2), min(y1, y2),	/* x y */
							/* width height */
				abs(x1 - x2), abs(y1 - y2),
							/* color width */
				GRAPHIC_COLOR, box->width,
				/* capstyle dashstyle dashlength dashspace */
				1, 0, -1, -1,
						/* filltype fillwidth */
				0, -1,
					/* angle1 pitch1 angle2 pitch2 */
				-1, -1, -1, -1);
			break;
		}
		case ARC_DRAW_TYPE: {
			LibraryDrawArc *arc = &(drawing->U.Arc);
			int x1, y1, r, t1, t2;

			x1 = shift + magn*arc->x;
			y1 = shift + magn*arc->y;
			r = magn*arc->r;
/* TODO: recheck result with various arcs */
printf("DBG: arc t1 %d t2 %d\n", arc->t1, arc->t2);
			t1 = round(arc->t1/10.0);
			if (t1 > 180)
				t1 -= 360;
			t2 = abs(round((arc->t2 - arc->t1)/10.0));
			if(t2 > 180)
				t2 -= 360;

			fprintf(fd, "A %d %d %d %d %d %d %d %d %d %d %d\n",
					x1, y1, r,	/* x y radius */
					t1, t2,	/* startangle sweepangle */
							/* color width */
					GRAPHIC_COLOR, arc->width,
					1,	/* capstyle */
					/* dashstyle dashlength dashspace */
					0, -1, -1);

			break;
		}
		case TEXT_DRAW_TYPE: {
			LibraryDrawText *text = &(drawing->U.Text);
			int x1, y1;

			x1 = shift + magn*text->x; 
			y1 = shift + magn*text->y; 
			fprintf(fd, "T %d %d %d %d %d %d %d %d %d\n%s\n",
					x1, y1,				/* x y */
					TEXT_COLOR,			/* color */
					text->size,		/* size (points) */
					1, 1,			/* visibility show_name_value */
					0, LOWER_LEFT, 1,	/* angle alignment num_lines */
					text->Text);		/* text */

			if (text->Horiz) {
				fprintf(stderr,
					"WARN: ignoring text Horiz %d\n",
						text->Horiz);
			}

			if (text->type) {
				fprintf(stderr,
					"WARN: ignoring text type %d\n",
						text->type);
			}
			break;
		}
		default:
			fprintf(stderr, "WARN: skipping DrawType %d in %s\n",
					drawing->DrawType, lib_en->Name);
		}

#if 0
		fprintf(stderr, "WARN: skipping Convert %d in %s\n",
					drawing->Convert, lib_en->Name);
#endif
	}

#if 0

	Drawings
	BBoxMinX, BBoxMaxX, BBoxMinY, BBoxMaxY
#endif


	fclose(fd);
	return 1;
}

int lib_create(const char *dir_name, LibraryStruct *lib)
{
	LibraryEntryStruct *lib_en;
	char *lib_dir_name;
	int i;

	if (lib->isSheet)
		return 0;

	i = sizeof(char)*(strlen(dir_name) + 1 + strlen(lib->Name) + 1);
	lib_dir_name = malloc(i);
	snprintf(lib_dir_name, i, "%s/%s", dir_name, lib->Name);

	if (!mkdir_or_exist(lib_dir_name)) {
		free(lib_dir_name);
		return 0;
	}

	lib_en = lib->Entries;
	for (i = 0; i < lib->NumOfParts; i++) {
		if (lib_en == NULL)
			break;

		if (lib_en->Type == ROOT) { 
			if (!lib_entry_create(lib_dir_name, lib_en)) {
				free(lib_dir_name);
				return 0;
			}
		} else {
			fprintf(stderr, "WARN: skip %s as non ROOT type in %s lib\n",
					lib_en->Name, lib->Name);
		}

printf("DBG #%d: type %d, part %s, prefix %s\n", i, lib_en->Type, lib_en->Name, lib_en->Prefix);
		lib_en = lib_en->nxt;
	}

	free(lib_dir_name);
	return 1;
}

void OutPro(LibraryStruct * libs)
{
printf("DBG: %s()\n", __func__);
}

FILE *FileSch = NULL;
char *FileSchName = NULL;

void CloseSch(void)
{
printf("DBG: %s()\n", __func__);
	if (FileSch == NULL)
		return;

	if (fclose(FileSch))
		fprintf(stderr, "can't close schematic file '%s'\n",
				FileSchName);
	free(FileSchName);
	FileSch = NULL;
}

void OpenSch(int x, int y)
{
	struct stat st;
	int i;
printf("DBG: %s()\n", __func__);

	CloseSch();	/* Close previous */

	i = sizeof(char)*(strlen(fName) + 4 + 1);
	FileSchName = malloc(i);
	snprintf(FileSchName, i, "%s.sch", fName);

	if (0 == stat(FileSchName, &st)) {
		fprintf(stderr, "schematic file exist '%s'\n", FileSchName);
		exit(2);
	} else if (errno != ENOENT) {
		fprintf(stderr, "can't stat() schematic file '%s', errno: %d\n",
				FileSchName, errno);
		exit(2);
	}

	FileSch = fopen(FileSchName, "w");
	if (FileSch == NULL) {
		fprintf(stderr, "can't create schematic file '%s'\n",
				FileSchName);
		exit(2);
	}

	fprintf(FileSch, "v %s %s\n", GEDA_VERSION, GEDA_FILE_VERSION);
}

static int no_open_file(FILE *fd, const char *func_name)
{
	if (FileSch == NULL) {
		fprintf(stderr, "no opened file for %s()\n", func_name);
		return 1;
	}

	return 0;
}

void OutConn(int x, int y)
{
/* TODO */
	/* Dot on connected wires, gschem add it automatically, but for
	 * consistensy this to be checked with wires end. */

printf("DBG: %s()\n", __func__);

	if (no_open_file(FileSch, __func__))
		return;

#if 0
	x = shift + magn*x;
	y = shift + magn*y;
printf("DBG: %s() %d;%d\n", __func__, x, y);

	fprintf(FileSch, "N %d %d %d %d %d\n",
		x, y, x+100, y+100,	/* x1 y1 x2 y2 */
		GRAPHIC_COLOR);	/* color width */

	/* TODO */
#endif
}

void OutInst(char *refcell, char *refdes, char *value, char *foot,
		char *mfgname, char *mfgpart,
		int ts, int ox, int oy, int rx, int ry, int vx, int vy,
		int rflg, int vflg, int unit, int *Rot)
{
printf("DBG: %s()\n", __func__);
}

void OutWire(int x1, int y1, int x2, int y2)
{
printf("DBG: %s()\n", __func__);

	if (no_open_file(FileSch, __func__))
		return;

	x1 = shift + magn*x1;
	y1 = shift + magn*y1;
	x2 = shift + magn*x2;
	y2 = shift + magn*y2;

	fprintf(FileSch, "N %d %d %d %d %d\n",
		x1, y1, x2, y2,	/* x1 y1 x2 y2 */
		NET_COLOR);	/* color width */
}

void OutText(int g, char *str, int x, int y, int size)
{
printf("DBG: %s()\n", __func__);

	if (str == NULL || *str == '\0') {
		fprintf(stderr, "empty text in %s()\n in '%s'",
				__func__, FileSchName);
		return;
	}

	if (no_open_file(FileSch, __func__))
		return;

	x = shift + magn*x;
	y = shift + magn*y;
	size = size/font_denom;

/* TODO: check OutText() in savelib.c */

/* TODO: add space before = if any to distinguish from attribute */

	fprintf(FileSch, "T %d %d %d %d %d %d %d %d %d\n",
			x, y,			/* x y */
			TEXT_COLOR,		/* color */
			size,			/* size (points) */
			1, 0,			/* visibility show_name_value */
			0, LOWER_LEFT, 1);	/* angle alignment num_lines */
/* TODO: remove debug info in string below */
	fprintf(FileSch, "%s DBG: g%d\n", str, g);
}

void OutSheets(struct pwr *pgs)
{
	for(; pgs != NULL; pgs = pgs->nxt) {
printf("DBG: %s %s\n", __func__, pgs->s);
	}
}

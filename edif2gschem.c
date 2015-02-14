/* edif2gschem -- EDIF to gEDA gschem converter */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <math.h>
#include <assert.h>

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

#ifdef DEBUG
# define dprintf(...) fprintf(stderr, __VA_ARGS__)
#else
# define dprintf(...)
#endif

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
	const char *field_id_str[] = {
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

void attrib_add(FILE *fd, const char *name, const char *value,
		int x, int y, int color, int size, int visible,
		int show_name_value, int angle, int alignment)
{
	/* TODO: num_lines set as number of '\n' */

	fprintf(fd, "T %d %d %d %d %d %d %d %d %d\n%s=%s\n",
		x, y,				/* x y */
		ATTRIBUTE_COLOR,		/* color */
		size,				/* size (points) */
		visible, show_name_value,	/* visibility show_name_value */
		angle, alignment, 1,		/* angle alignment num_lines */
		name, value);			/* name value */
}

void text_add(FILE *fd, const char *text,
		int x, int y, int color, int size, int visible,
		int show_name_value, int angle, int alignment)
{
	/* TODO: num_lines set as number of '\n' */

	fprintf(fd, "T %d %d %d %d %d %d %d %d %d\n%s\n",
		x, y,			/* x y */
		color,			/* color */
		size,			/* size (points) */
		visible, show_name_value,	/* visibility show_name_value */
		angle, alignment, 1,	/* angle alignment num_lines */
		text);			/* text */
}

int font_size(int size)
{
	const int font_denom = 13;

	size /= font_denom;
	if (size != 0)
		return size;

	return 4;
}

int lib_entry_create(const char *dir_name, LibraryEntryStruct *lib_en)
{
	char *file_name;
	FILE *fd;
	struct stat st;
	LibraryFieldEntry *field;
	LibraryDrawEntryStruct *drawing;
	char *str;
	int pinseq;
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
	attrib_add(fd, "refdes", lib_en->Prefix,
		magn*lib_en->PrefixPosX, magn*lib_en->PrefixPosY, ATTRIBUTE_COLOR,
		font_size(lib_en->PrefixSize), lib_en->DrawPrefix, 1, 0, LOWER_LEFT);
	attrib_add(fd, "device", lib_en->Name,
		magn*lib_en->NamePosX, magn*lib_en->NamePosY, ATTRIBUTE_COLOR,
		font_size(lib_en->NameSize), lib_en->DrawName, 1, 0, LOWER_LEFT);

fprintf(stderr, "WARN: skipping AliasList '%s'\n", lib_en->AliasList);
fprintf(stderr, "WARN: skipping NameOrient %d, PrefixOrient %d\n",
		lib_en->NameOrient, lib_en->PrefixOrient);
fprintf(stderr, "WARN: skipping NumOfUnits %d\n", lib_en->NumOfUnits);
fprintf(stderr, "WARN: skipping TextInside %d\n", lib_en->TextInside);

	for (field = lib_en->Fields; field != NULL; field = field->nxt) {
		if ((field->Text == NULL) || (strlen(field->Text) == 0))
			continue;

		attrib_add(fd, field_id_str(field->FieldId), field->Text,
			magn*field->PosX, magn*field->PosY, ATTRIBUTE_COLOR,
			font_size(field->Size), !field->Flags & TEXT_NO_VISIBLE,
			0, 0, LOWER_LEFT);

fprintf(stderr, "WARN: skipping Orient %d\n", field->Orient);
	}

	pinseq = 1;
	for (drawing = lib_en->Drawings;
				drawing != NULL; drawing = drawing->nxt) {
		switch (drawing->DrawType) {
		case CIRCLE_DRAW_TYPE: {
			LibraryDrawCircle *circle = &(drawing->U.Circ);
			int x1, y1;

			x1 = magn*circle->x;
			y1 = magn*circle->y;
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
			int x1, x2, y1, y2;
			char str_buf[1024];

			switch (pin->Orient) {
			case 'R':
				x1 = magn*pin->posX;
				y1 = magn*pin->posY;
				x2 = x1 + magn*pin->Len;
				y2 = y1;
				number_align = LOWER_LEFT;
				label_align = MIDDLE_LEFT;
				break;
			case 'U':
				x1 = magn*pin->posX;
				y1 = magn*pin->posY;
				x2 = x1;
				y2 = y1 + magn*pin->Len;
				angle = 90;
				number_align = LOWER_LEFT;
				label_align = MIDDLE_LEFT;
				break;
			case 'D':
				x1 = magn*pin->posX;
				y1 = magn*pin->posY;
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
			attrib_add(fd, "pinnumber", pin->Num, 
					magn*pin->posX, magn*pin->posY, ATTRIBUTE_COLOR,
					font_size(pin->SizeNum), lib_en->DrawPinNum,
					1, angle, number_align);
			assert(sizeof(str_buf) > snprintf(str_buf, sizeof(str_buf), "%d", pinseq++));
			attrib_add(fd, "pinseq", str_buf,
					magn*pin->posX, magn*pin->posY, ATTRIBUTE_COLOR,
					4, 0,
					0, angle, MIDDLE_MIDDLE);

			str = pin_name_format((pin->ReName == NULL)?
						pin->Name: pin->ReName);
			if (str == NULL) {
				str = strdup((pin->ReName == NULL)?
							pin->Name: pin->ReName);
			} 
			attrib_add(fd, "pinlabel", str,
					x2, y2,
					ATTRIBUTE_COLOR,
					font_size(pin->SizeName), lib_en->DrawPinName,
					1, angle, label_align);
			free(str);

			attrib_add(fd, "pintype", pintype_str(pin->PinType),
					magn*pin->posX, magn*pin->posY, ATTRIBUTE_COLOR,
					4, 0,
					0, angle, MIDDLE_MIDDLE);

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

			x1 = magn*coords[0];
			y1 = magn*coords[1];
			coords += 2;

			for (i = 1; i < poly->n; i++) {
				x2 = magn*coords[0];
				y2 = magn*coords[1];
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

			x1 = magn*box->x1;
			y1 = magn*box->y1;
			x2 = magn*box->x2;
			y2 = magn*box->y2;
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

			x1 = magn*arc->x;
			y1 = magn*arc->y;
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

			text_add(fd, text->Text, magn*text->x, magn*text->y,
					TEXT_COLOR, font_size(text->size), 1,
					1, 0, LOWER_LEFT);

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
	if (fd == NULL) {
		dprintf("no opened file for %s()\n", func_name);
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

void OutInst(char *refcell, char *refdes, char *value, char *footprint,
		char *mfgname, char *mfgpart,
		int txtsize, int ox, int oy, int rx, int ry, int vx, int vy,
		int rflg, int vflg, int unit, int Rot[2][2])
{
	int angle = 0, mirror = 0;

	if (Rot[0][0] == 0) {
		if (Rot[0][1] == -1)
			angle = 90;
		else
			angle = 270;
	} else if (Rot[0][0] == 1) {
		if (Rot[1][1] == 1) {
			angle = 180;
			mirror = 1;
		}
	} else if (Rot[0][0] == -1) {
		if (Rot[1][1] == -1)
			mirror = 1;
		else
			angle = 180;
	}

	/* TODO: rflag, vflag are flags for refdes and value?*/
	/* TODO: unit != 1 for multipart */

	dprintf("%s\tRot\t%d %d %d %d\n", refdes, Rot[0][0], Rot[0][1], Rot[1][0], Rot[1][1]);

	if (no_open_file(FileSch, __func__))
		return;

	fprintf(FileSch, "C %d %d %d %d %d %s.sym\n",
		shift + magn*ox, shift + magn*oy, 1,	/* x y selectable */
		angle, mirror, refcell);		/* angle mirror basename */

	txtsize = font_size(txtsize);

	fprintf(FileSch, "{\n");
	if (refdes != NULL && refdes[0] != '\0')
		attrib_add(FileSch, "refdes", refdes, shift + magn*rx, shift + magn*ry,
			ATTRIBUTE_COLOR, txtsize, 1, 1, 0, LOWER_LEFT);
	if (value != NULL && value[0] != '\0')
		attrib_add(FileSch, "value", value, shift + magn*vx, shift + magn*vy,
			ATTRIBUTE_COLOR, txtsize, 1, 1, 0, UPPER_LEFT);
	if (footprint != NULL && footprint[0] != '\0')
		attrib_add(FileSch, "footprint", footprint, shift + magn*ox, shift + magn*oy,
			ATTRIBUTE_COLOR, txtsize, 0, 1, 0, LOWER_LEFT);
	if (mfgname != NULL && mfgname[0] != '\0')
		attrib_add(FileSch, "mfgname", mfgname, shift + magn*ox, shift + magn*oy,
			ATTRIBUTE_COLOR, txtsize, 0, 1, 0, LOWER_LEFT);
	if (mfgpart != NULL && mfgpart[0] != '\0')
		attrib_add(FileSch, "mfgpart", mfgpart, shift + magn*ox, shift + magn*oy,
			ATTRIBUTE_COLOR, txtsize, 0, 1, 0, LOWER_LEFT);
	fprintf(FileSch, "}\n");
}

void OutWire(int x1, int y1, int x2, int y2)
{
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

void OutText(OutTextType type, char *str, int x, int y, int size)
{
	if (str == NULL || *str == '\0') {
		fprintf(stderr, "empty text in %s()\n in '%s'",
				__func__, FileSchName);
		return;
	}

	if (no_open_file(FileSch, __func__))
		return;

/* TODO: check OutText() in savelib.c */

/* TODO: add space before = if any to distinguish from attribute */

	switch (type) {
	default:
	case TEXT_LABEL_GLOBAL:
/* TODO: remove debug info in string below */
		fprintf(FileSch, "%s DBG: type%d\n", str, type);
		/* no break */
	case TEXT_LABEL_NORMAL:
		/* TODO: this is net label? */
		/* no break */
	case TEXT_TEXT:
		text_add(FileSch, str, shift + magn*x, shift + magn*y,
				TEXT_COLOR, font_size(size), 1,
				0, 0, LOWER_LEFT);
		break;
	}

}

void OutSheets(struct pwr *pgs)
{
	for(; pgs != NULL; pgs = pgs->nxt) {
printf("DBG: %s %s\n", __func__, pgs->s);
	}
}

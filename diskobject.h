struct IconPixmaps;

extern void init_iconpalette(void);
extern void load_do(const char *filename, struct IconPixmaps *ip);
extern void set_sys_palette(void);

extern int iconcolormask;

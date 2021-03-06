#ifndef SETTINGS_FILE_SETTINGS_H_
#define SETTINGS_FILE_SETTINGS_H_

#include <stdio.h>

#define SETTING_MAX_KEY_SIZE 255
#define SETTING_MAX_VAL_SIZE 255

#if defined (__cplusplus)
extern "C" {
#endif

typedef struct setting_s {
	char key[SETTING_MAX_KEY_SIZE];
	char val[SETTING_MAX_VAL_SIZE];

	/* linked list */
	struct setting_s *next;

} setting_t;

typedef struct settings_file_s {
	FILE *fid;
	char *fname;

	setting_t *first;

} settings_file_t;


settings_file_t* settings_file_open(const char *filename);
void settings_file_close(settings_file_t* sf);
const char* settings_file_get_value(settings_file_t *sf, const char *key);
void settings_file_print(settings_file_t *sf);
int settings_file_num_settings(settings_file_t *sf);
const char* settings_file_get_key_by_index(settings_file_t *sf, size_t index);

#if defined (__cplusplus)
}
#endif

#endif /* SETTINGS_FILE_SETTINGS_H_ */

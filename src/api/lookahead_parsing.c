#include "lookahead_parsing.h"

int ParseLookaheadJsonFile(const char * file_path,
    struct lookahead_map ** lmap) {
    const cJSON * start_offset;
    const cJSON * finish_offset;
    const cJSON * lookaheads;
    const cJSON * lookahead;
    int status = 0;
    char * file_content = 0;
    long length;
    char offset_string[MAX_OFFSET_DIGITS];
    FILE * f = fopen (file_path, "rb");

    printf("Parsing lookahead file: %s...\n", file_path);
    if (f) {
        fseek (f, 0, SEEK_END);
        length = ftell (f);
        fseek (f, 0, SEEK_SET);
        file_content = malloc (length);
        if (file_content)
        {
            fread (file_content, 1, length, f);
        }
        fclose (f);
    }

    if (!length || !file_content) {
        printf("Lookahead file empty! Ignoring ...\n");
        return 0;
    }

    cJSON *lookahead_json = cJSON_Parse(file_content);
    if (lookahead_json == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            fprintf(stderr, "Json parse error before: %s\n", error_ptr);
        }
        status = 0;
        goto end;
    }

    start_offset = cJSON_GetObjectItemCaseSensitive(
        lookahead_json, "start_offset");
    finish_offset = cJSON_GetObjectItemCaseSensitive(
        lookahead_json, "finish_offset");
    if (!cJSON_IsNumber(start_offset) || !cJSON_IsNumber(finish_offset)) {
       printf("Json Error: Lookahead offset values must be integers !\n");
       status = 0;
       goto end;
    }
    printf("Start Offset: %lu, Finish Offset: %lu\n",
        (long)cJSON_GetNumberValue(start_offset),
        (long)cJSON_GetNumberValue(finish_offset));

    if (cJSON_GetNumberValue(start_offset) < 0 ||
        cJSON_GetNumberValue(finish_offset) < 0) {
        printf("Json Error: Start/finish offsets must be positive !\n");
        status = 0;
        goto end;
    }
    int number_of_lookahead_values = 
        cJSON_GetNumberValue(finish_offset) - cJSON_GetNumberValue(start_offset) + 1;
    if (number_of_lookahead_values < 0) {
       printf("Json Error: finish offset cannot be less than start offset !\n");
       status = 0;
       goto end; 
    }

    *lmap = (struct lookahead_map *)malloc(sizeof(struct lookahead_map));
    if (!lmap) {
        printf("Failed to allot storage for lookahead map !\n");
        free(file_content);
        return 0;
    }
    (*lmap)->lookaheads = (s64 *)malloc(sizeof(s64)*number_of_lookahead_values);
    if (!(*lmap)->lookaheads) {
        printf("Failed to allot storage for lookaheads !\n");
        free(*lmap);
        *lmap = NULL;
        free(file_content);
        return 0;
    }
    (*lmap)->start_offset = (long)cJSON_GetNumberValue(start_offset);
    (*lmap)->finish_offset = (long)cJSON_GetNumberValue(finish_offset);
    (*lmap)->num_entries = (long) number_of_lookahead_values;

    lookaheads = cJSON_GetObjectItemCaseSensitive(lookahead_json,
        "lookaheads");
    for (long i = (*lmap)->start_offset;  i <= (*lmap)->finish_offset; i++) {
        memset(offset_string, 0, MAX_OFFSET_DIGITS);
        sprintf(offset_string, "%lu", i);
        lookahead = cJSON_GetObjectItemCaseSensitive(lookaheads,
            offset_string);
        if (!cJSON_IsNull(lookahead) && cJSON_IsNumber(lookahead)) {
            (*lmap)->lookaheads[i - (*lmap)->start_offset] = 
                (long)cJSON_GetNumberValue(lookahead);
        } else {
            (*lmap)->lookaheads[i - (*lmap)->start_offset] = 0;
        }
    }
    status = 1;
end:
    cJSON_Delete(lookahead_json);
    free(file_content);
    return status;
}

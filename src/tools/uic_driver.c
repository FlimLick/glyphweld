#include "glyphweld/ui_compiler.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <sys/stat.h>
#define GW_OPEN _open
#define GW_CLOSE _close
#define GW_GETPID _getpid
#define GW_O_BINARY _O_BINARY
#define GW_STAT _stat
#define GwStat struct _stat
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define GW_OPEN open
#define GW_CLOSE close
#define GW_GETPID getpid
#define GW_O_BINARY 0
#define GW_STAT stat
#define GwStat struct stat
#endif

typedef struct OutputState
{
  const char *final_path;
  char temp_path[1024];
  char backup_path[1024];
  int original_existed;
  int original_backed_up;
  int new_installed;
  int temp_created;
} OutputState;

static int should_fail(const char *step)
{
  static const char *cursor;
  const char *value = getenv("GW_UIC_DRIVER_FAIL_STEPS");
  size_t length = strlen(step);
  if (cursor == NULL)
    cursor = value;
  if (cursor == NULL || strncmp(cursor, step, length) != 0 ||
      (cursor[length] != '\0' && cursor[length] != ','))
    return 0;
  cursor += length;
  if (*cursor == ',')
    ++cursor;
  return 1;
}

static int read_file(const char *path, uint8_t **bytes, uint64_t *size)
{
  FILE *file = fopen(path, "rb");
  long length;
  if (file == NULL || fseek(file, 0L, SEEK_END) != 0 || (length = ftell(file)) < 0 ||
      fseek(file, 0L, SEEK_SET) != 0)
  {
    if (file != NULL)
      (void)fclose(file);
    return 0;
  }
  *bytes = (uint8_t *)malloc((size_t)length);
  if (*bytes == NULL || fread(*bytes, 1U, (size_t)length, file) != (size_t)length)
  {
    free(*bytes);
    (void)fclose(file);
    return 0;
  }
  if (fclose(file) != 0)
  {
    free(*bytes);
    return 0;
  }
  *size = (uint64_t)length;
  return 1;
}

static int write_file(const char *path, const uint8_t *bytes, uint64_t size, const char *step)
{
  FILE *file;
  int ok = 1;
  if (should_fail(step))
    return 0;
  file = fopen(path, "wb");
  if (file == NULL)
    return 0;
  if (fwrite(bytes, 1U, (size_t)size, file) != (size_t)size)
    ok = 0;
  if (ok && fflush(file) != 0)
    ok = 0;
  if (fclose(file) != 0)
    ok = 0;
  return ok;
}

static int make_path(char *out, size_t capacity, const char *base, const char *kind,
                     unsigned long nonce)
{
  int written = snprintf(out, capacity, "%s.gw-%s-%lu", base, kind, nonce);
  return written >= 0 && (size_t)written < capacity;
}

static int reserve_temp(OutputState *state, const char *kind, unsigned long nonce, int *owned)
{
  int fd;
  char *path = strcmp(kind, "tmp") == 0 ? state->temp_path : state->backup_path;
  unsigned long attempt;
  for (attempt = 0U; attempt < 64U; ++attempt)
  {
    if (!make_path(path, sizeof(state->temp_path), state->final_path, kind, nonce + attempt))
      return 0;
    fd = GW_OPEN(path, O_WRONLY | O_CREAT | O_EXCL | GW_O_BINARY, 0600);
    if (fd >= 0)
    {
      *owned = 1;
      return GW_CLOSE(fd) == 0;
    }
    if (errno != EEXIST)
      return 0;
  }
  return 0;
}

static int path_state(const char *path)
{
  GwStat info;
  if (GW_STAT(path, &info) == 0)
    return 1;
  return errno == ENOENT ? 0 : -1;
}

static int backup_output(OutputState *state, const char *step)
{
  if (!state->original_existed)
    return 1;
  if (should_fail(step) || rename(state->final_path, state->backup_path) != 0)
    return 0;
  state->original_backed_up = 1;
  return 1;
}

static const char *basename_of(const char *path)
{
  const char *slash = strrchr(path, '/');
  const char *backslash = strrchr(path, '\\');
  if (backslash != NULL && (slash == NULL || backslash > slash))
    slash = backslash;
  return slash == NULL ? path : slash + 1;
}

static int header_name_matches(const GwUiCompilerResult *result, const char *path)
{
  const char prefix[] = "#include \"";
  const char *source = (const char *)result->source_bytes;
  const char *include = strstr(source, prefix);
  const char *end;
  if (include == NULL)
    return 0;
  include += sizeof(prefix) - 1U;
  end = strchr(include, '"');
  return end != NULL && (size_t)(end - include) == strlen(basename_of(path)) &&
         memcmp(include, basename_of(path), (size_t)(end - include)) == 0;
}

int main(int argc, char **argv)
{
  uint8_t *input = NULL;
  uint64_t input_size;
  GwUiCompilerResult result = {0};
  GwUiCompilerRequest request;
  OutputState header = {0}, source = {0};
  int header_backup_owned = 0, source_backup_owned = 0;
  unsigned long nonce =
      (unsigned long)clock() ^ (unsigned long)time(NULL) ^ (unsigned long)GW_GETPID();
  if (argc != 4 || !read_file(argv[1], &input, &input_size))
  {
    fprintf(stderr, "glyphweld_uic_driver: cannot read '%s'\n", argc > 1 ? argv[1] : "input");
    return 1;
  }
  header.final_path = argv[2];
  source.final_path = argv[3];
  if (!reserve_temp(&header, "tmp", nonce, &header.temp_created) ||
      !reserve_temp(&source, "tmp", nonce + 1U, &source.temp_created) ||
      !reserve_temp(&header, "bak", nonce + 2U, &header_backup_owned) ||
      !reserve_temp(&source, "bak", nonce + 3U, &source_backup_owned))
  {
    fprintf(stderr, "glyphweld_uic_driver: cannot reserve unique temporary paths\n");
    goto cleanup;
  }
  (void)remove(header.backup_path);
  (void)remove(source.backup_path);
  request = (GwUiCompilerRequest){
      (uint32_t)sizeof(request), argv[1], (uint32_t)strlen(argv[1]), input, input_size, NULL, NULL};
  if (gw_ui_compiler_compile(&request, &result) != GW_UIC_ERROR_NONE)
  {
    fprintf(stderr, "%s:%u:%u: %s\n", argv[1], result.diagnostic.line, result.diagnostic.column,
            result.diagnostic.message == NULL ? "compile failed" : result.diagnostic.message);
    goto cleanup;
  }
  if (!header_name_matches(&result, header.final_path))
  {
    fprintf(stderr, "glyphweld_uic_driver: header basename must match generated include name\n");
    goto cleanup;
  }
  if (!write_file(header.temp_path, result.header_bytes, result.header_size, "header-write") ||
      !write_file(source.temp_path, result.source_bytes, result.source_size, "source-write"))
  {
    fprintf(stderr, "glyphweld_uic_driver: temporary write failed\n");
    goto cleanup;
  }
  header.original_existed = path_state(header.final_path);
  source.original_existed = path_state(source.final_path);
  if (header.original_existed < 0 || source.original_existed < 0)
  {
    fprintf(stderr, "glyphweld_uic_driver: output metadata check failed\n");
    goto cleanup;
  }
  if (!backup_output(&header, "header-backup"))
  {
    fprintf(stderr, "glyphweld_uic_driver: header backup failed\n");
    goto rollback;
  }
  if (header.original_backed_up)
    header_backup_owned = 0;
  if (!backup_output(&source, "source-backup"))
  {
    fprintf(stderr, "glyphweld_uic_driver: source backup failed\n");
    goto rollback;
  }
  if (source.original_backed_up)
    source_backup_owned = 0;
  if (should_fail("header-install") || rename(header.temp_path, header.final_path) != 0)
  {
    fprintf(stderr, "glyphweld_uic_driver: header install failed\n");
    goto rollback;
  }
  header.new_installed = 1;
  if (should_fail("source-install") || rename(source.temp_path, source.final_path) != 0)
  {
    fprintf(stderr, "glyphweld_uic_driver: source install failed\n");
    goto rollback;
  }
  source.new_installed = 1;
  (void)remove(header.backup_path);
  (void)remove(source.backup_path);
  gw_ui_compiler_result_dispose(&result);
  free(input);
  return 0;
rollback:
  if (header.new_installed)
    (void)remove(header.final_path);
  if (source.new_installed)
    (void)remove(source.final_path);
  if (header.original_backed_up &&
      (should_fail("header-restore") || rename(header.backup_path, header.final_path) != 0))
    fprintf(stderr, "glyphweld_uic_driver: header restoration failed; preserved backup '%s'\n",
            header.backup_path);
  if (source.original_backed_up &&
      (should_fail("source-restore") || rename(source.backup_path, source.final_path) != 0))
    fprintf(stderr, "glyphweld_uic_driver: source restoration failed; preserved backup '%s'\n",
            source.backup_path);
cleanup:
  if (header.temp_created)
    (void)remove(header.temp_path);
  if (source.temp_created)
    (void)remove(source.temp_path);
  if (header_backup_owned)
    (void)remove(header.backup_path);
  if (source_backup_owned)
    (void)remove(source.backup_path);
  gw_ui_compiler_result_dispose(&result);
  free(input);
  return 1;
}

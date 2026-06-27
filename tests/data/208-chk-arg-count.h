
typedef struct _some_t _some_t;

int chk_v (void);
int chk_a (int stuff);
int chk_b (int junk, const char *str);
int chk_c (int junk, const char *str, _some_t *s);
/* multi-line */
int chk_d (int junk,
         const char *str,
         _some_t *s);
/* multi-line, other stuff */
extern FILE *chk_e (const char *__restrict __filename,
  const char *__restrict __modes,
  FILE *__restrict __stream) __wur __nonnull ((3))
  ;

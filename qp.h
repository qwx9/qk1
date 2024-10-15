typedef union Trie Trie;
#pragma incomplete Trie

int qpget(Trie *t, char *k, int len, char **pk, void **pv);
int qpnext(Trie *t, char **pk, int *plen, void **pv);
Trie *qpdel(Trie *t, char *k, int len, char **pk, void **pv);
Trie *qpset(Trie *t, char *k, int len, void *v);

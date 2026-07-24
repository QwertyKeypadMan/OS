#ifndef _KAYAOS_SEMAPHORE_H
#define _KAYAOS_SEMAPHORE_H

/* KayaOS gercek thread/senkronizasyon konsepti olmayan tek-cekirdekli,
 * tek-gorevli bir kernel. TCC'nin paralel derleme (multi-thread) destegi
 * icin bekledigi <semaphore.h> API'sini burada SADECE derleme zamaninda
 * cozulmesi icin minimal olarak tanimliyoruz -- gercek fonksiyonlar
 * tcc_port.c'de no-op (sem_init/sem_wait/sem_post hep basari donuyor,
 * kilitleme yapmiyor, zaten tek thread oldugu icin gerek de yok). */

typedef int sem_t;

int sem_init(sem_t *sem, int pshared, unsigned int value);
int sem_destroy(sem_t *sem);
int sem_wait(sem_t *sem);
int sem_trywait(sem_t *sem);
int sem_post(sem_t *sem);
int sem_getvalue(sem_t *sem, int *sval);

#endif /* _KAYAOS_SEMAPHORE_H */
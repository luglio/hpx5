#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <arpa/inet.h>

#include "util.h"

#ifdef DEBUG
time_t _tictoc(time_t stime, int proc) {
  time_t etime;
  etime = time(NULL);
  if ((etime - stime) > 10) {
    if( proc >= 0 )
      fprintf(stderr, "Still waiting for a recv buffer from %d", proc);
    else
      fprintf(stderr, "Still waiting for a recv buffer from any peer");
    stime = etime;
  }
  return stime;
}
#endif

void photon_gettime_(double *s) {

  struct timeval tp;

  gettimeofday(&tp, NULL);
  *s = ((double)tp.tv_sec) + ( ((double)tp.tv_usec) / 1000000.0);
  return;
}

const char *photon_addr_getstr(photon_addr *addr, int af) {
  char *buf = malloc(40);
  return inet_ntop(AF_INET6, addr->raw, buf, 40);
}
  
int photon_scan(const char *s, int c, int len) {
  int i;
  for (i = 0; !len || (i < len); ++i) {
    if ((s[i] == '\0') || (s[i] == c)) return i;
  }
  return len;
}

// Same approach taken by GASNET ibv-conduit 
// https://bitbucket.org/berkeleylab/gasnet/src/d62ccc8062648c50b245028deb7670f94a2ebeea/ibv-conduit/gasnet_core.c?at=master
int photon_parse_devstr(const char *devstr, photon_dev_list **ret_list) {
  int ndev = 0;
  int len = strlen(devstr);
  photon_dev_list *dlist = NULL;
  while (len > 0) {
    int a = photon_scan(devstr, '+', len);
    int b = photon_scan(devstr, ':', a);
    if (b >= a-1) {
      photon_dev_list *d = malloc(sizeof(photon_dev_list));
      d->next = dlist;
      dlist = d;
      d->dev = strndup(devstr, b);
      d->port = 1;
      ndev++;
    }
    else {
      const char *r = devstr + b + 1;
      while (r < devstr + a) {
	int port = atoi(r);
	if (port) {
	  photon_dev_list *d = malloc(sizeof(photon_dev_list));
	  d->next = dlist;
	  dlist = d;
	  d->dev = strndup(devstr, b);
	  d->port = port;
	  ndev++;
	  r += photon_scan(r, ',', 0) + 1;
	}
	else {
	  // we didn't get a port num
	  return -1;
	}
      }
    }
    devstr += a + 1;
    len -= a + 1;
  }
  
  *ret_list = dlist;
  return ndev;
}

int photon_match_dev(photon_dev_list *dlist, const char *dev, int port) {
  photon_dev_list *curr;
  int found = 0;

  if (dlist != NULL) {
    for (curr = dlist; curr && !found; curr = curr->next) {
      if (!strcmp(dev, curr->dev)) {
        if (!port) { /* match HCA only */
          found = 1;
        } else if (!curr->port || (curr->port == port)) { /* 0 is a wild card */
          found = 1;
        }
      }
    }
  } else {
    found = 1;
  }
  
  return found;
}

void photon_free_devlist(photon_dev_list *d) {
  photon_dev_list *iter;
  while (d) {
    iter = d;
    d = d->next;
    free(iter->dev);
    free(iter);
  }
}
  
    

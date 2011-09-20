#include "socket.h"
#include "tcp_hash.h"
#include "tcp.h"
#include "ip.h"
#include "netif.h"

static struct tcp_hash_table tcp_table;
/* @src is for remote machine */
static struct sock *tcp_lookup_sock_establish(unsigned int src, unsigned int dst,
				unsigned short src_port, unsigned short dst_port)
{
	struct hlist_head *head;
	struct hlist_node *node;
	struct sock *sk;
	head = tcp_ehash_head(tcp_ehashfn(src, dst, src_port, dst_port));
	hlist_for_each_sock(sk, node, head) {
		if (sk->sk_saddr == dst &&
			sk->sk_daddr == src &&
			sk->sk_sport == dst_port &&
			sk->sk_dport == src_port)
			return get_sock(sk);
	}
	return NULL;
}

static struct sock *tcp_lookup_sock_listen(unsigned int addr, unsigned int nport)
{
	struct hlist_head *head = tcp_lhash_head(ntohs(nport) & TCP_LHASH_MASK);
	struct hlist_node *node;
	struct sock *sk;

	hlist_for_each_sock(sk, node, head) {
		if ((!sk->sk_saddr || sk->sk_saddr == addr) &&
			sk->sk_sport == nport)
			return get_sock(sk);
	}
	return NULL;
}

/* port is net order! */
struct sock *tcp_lookup_sock(unsigned int src, unsigned int dst,
				unsigned int src_port, unsigned int dst_port)
{
	struct sock *sk;
	sk = tcp_lookup_sock_establish(src, dst, src_port, dst_port);
	if (!sk)
		sk = tcp_lookup_sock_listen(dst, dst_port);
	return sk;
}

static int tcp_send_buf()
{
/*
                 Generally, an interactive application protocol must set
                 the PUSH flag at least in the last SEND call in each
                 command or response sequence.  A bulk transfer protocol
                 like FTP should set the PUSH flag on the last segment
                 of a file or when necessary to prevent buffer deadlock.
 */
	return 0;
}

static int tcp_recv()
{
/*
                 At the receiver, the PSH bit forces buffered data to be
                 delivered to the application (even if less than a full
                 buffer has been received). Conversely, the lack of a
                 PSH bit can be used to avoid unnecessary wakeup calls
                 to the application process; this can be an important
                 performance optimization for large timesharing hosts.
 */
	return 0;
}

static _inline int __tcp_port_used(unsigned short nport, struct hlist_head *head)
{
	struct hlist_node *node;
	struct tcp_sock *tsk;
	for_each_tcp_sock(tsk, node, head)
		if (tsk->sk.sk_sport == nport)
			return 1;
	return 0;
}

static _inline int tcp_port_used(unsigned short nport)
{
	return __tcp_port_used(nport,
		tcp_bhash_head(ntohs(nport) & TCP_BHASH_MASK));
}

static unsigned short tcp_get_port(void)
{
	static unsigned short defport = TCP_BPORT_MIN;
	unsigned short nport = 0;
	/* no free bind port resource */
	if (tcp_table.bfree <= 0)
		return 0;
	/* assert that we can break the loop */
	while (tcp_port_used(htons(defport))) {
		if (++defport > TCP_BPORT_MAX)
			defport = TCP_BPORT_MIN;
	}
	nport = htons(defport);
	if (++defport > TCP_BPORT_MAX)
		defport = TCP_BPORT_MIN;
	return nport;
}

static void tcp_bhash(struct tcp_sock *tsk)
{
	get_sock(&tsk->sk);
	hlist_add_head(&tsk->bhash_list, tcp_bhash_head(tsk->bhash));
}

static int tcp_set_sport(struct sock *sk, unsigned short nport)
{
	int err = -1;

	if ((nport && tcp_port_used(nport)) ||
		(!nport && !(nport = tcp_get_port())))
		goto out;
	tcp_table.bfree--;
	sk->sk_sport = nport;
	tcpsk(sk)->bhash = ntohs(nport) & TCP_BHASH_MASK;
	tcp_bhash(tcpsk(sk));
	err = 0;
out:
	return err;
}

static void tcp_unset_sport(struct sock *sk)
{
	tcp_table.bfree++;
}

static void tcp_unbhash(struct tcp_sock *tsk)
{
	if (!hlist_unhashed(&tsk->bhash_list)) {
		tcp_unset_sport(&tsk->sk);
		hlist_del(&tsk->bhash_list);
		free_sock(&tsk->sk);
	}
}

int tcp_hash(struct sock *sk)
{
	struct tcp_sock *tsk = tcpsk(sk);
	struct hlist_head *head;
	unsigned int hash;

	if (tsk->state == TCP_CLOSED)
		return -1;
	if (tsk->state == TCP_LISTEN) {
		sk->hash = ntohs(sk->sk_sport) & TCP_LHASH_MASK;
		head = tcp_lhash_head(sk->hash);
		/*
		 * We dont need to check conflict of listen hash
		 * bind hash has done it for us.
		 */
	} else {
		hash = tcp_ehashfn(sk->sk_saddr, sk->sk_daddr,
				sk->sk_sport, sk->sk_dport);
		head = tcp_ehash_head(hash);
		if (tcp_ehash_conflict(head, sk))
			return -1;
		sk->hash = hash;
	}
	sock_add_hash(sk, head);
	return 0;
}

static void tcp_unhash(struct sock *sk)
{
	sock_del_hash(sk);
	sk->hash = 0;
}

static _inline void tcp_pre_wait_connect(struct tcp_sock *tsk)
{
	tsk->wait_connect = &tsk->sk.sock->sleep;
}

static int tcp_wait_connect(struct tcp_sock *tsk)
{
	int err;
	err = sleep_on(tsk->wait_connect);
	tsk->wait_connect = NULL;
	return err;
}

static int tcp_connect(struct sock *sk, struct sock_addr *skaddr)
{
	struct tcp_sock *tsk = tcpsk(sk);
	int err;
	if (tsk->state != TCP_CLOSED)
		return -1;
	sk->sk_daddr = skaddr->dst_addr;	
	sk->sk_dport = skaddr->dst_port;
	/* three-way handshake starts, send first SYN */
	tsk->state = TCP_SYN_SENT;
	tsk->iss = alloc_new_iss();
	tsk->snd_una = tsk->iss;
	tsk->snd_nxt = tsk->iss + 1;
	if (tcp_hash(sk) < 0) {
		tsk->state = TCP_CLOSED;
		return -1;
	}
	/* 
	 * Race condition:
	 *  If we connect to localhost, then we will send syn
	 *  and recv packet. It wakes up tsk->wait_connect without
	 *  seting wait_connect.
	 *
	 * Fix:
	 *  set wait_connect before sending syn
	 */
	tcp_pre_wait_connect(tsk);
	tcp_send_syn(tsk, NULL);
	err = tcp_wait_connect(tsk);
	if (err || tsk->state != TCP_ESTABLISHED) {
		tcp_unhash(sk);
		tcp_unbhash(tsk);
		tsk->state = TCP_CLOSED;
		err = -1;
	}
	return err;
}

static int tcp_listen(struct sock *sk, int backlog)
{
	struct tcp_sock *tsk = tcpsk(sk);
	unsigned int oldstate = tsk->state;
	if (!sk->sk_sport)	/* no bind */
		return -1;
	if (backlog > TCP_MAX_BACKLOG)
		return -1;
	if (oldstate != TCP_CLOSED && oldstate != TCP_LISTEN)
		return -1;
	tsk->backlog = backlog;
	tsk->state = TCP_LISTEN;
	/* add tcpsk into listen hash table */
	if (oldstate != TCP_LISTEN && sk->ops->hash)
		sk->ops->hash(sk);
	return 0;
}

static int tcp_wait_accept(struct tcp_sock *tsk)
{
	int err;
	tsk->wait_accept = &tsk->sk.sock->sleep;
	err = sleep_on(tsk->wait_accept);
	tsk->wait_accept = NULL;
	return err;
}

static struct sock *tcp_accept(struct sock *sk)
{
	struct tcp_sock *tsk = tcpsk(sk);
	struct tcp_sock *newtsk = NULL;

	while (list_empty(&tsk->accept_queue)) {
		if (tcp_wait_accept(tsk) < 0)
			goto out;
	}
	newtsk = tcp_accept_dequeue(tsk);
	free_sock(&newtsk->sk);
	/* disassociate it with parent */
	free_sock(&newtsk->parent->sk);
	newtsk->parent = NULL;
out:
	return newtsk ? &newtsk->sk : NULL;
}

static void tcp_clear_listen_queue(struct tcp_sock *tsk)
{	
	struct tcp_sock *ltsk;
	while (!list_empty(&tsk->listen_queue)) {
		ltsk = list_first_entry(&tsk->listen_queue, struct tcp_sock, list);
		list_del_init(&ltsk->list);
		if (ltsk->state == TCP_SYN_RECV) {
			free_sock(&ltsk->parent->sk);
			ltsk->parent = NULL;
			tcp_unhash(&ltsk->sk);
			free_sock(&ltsk->sk);
		}
		/* FIXME: Why other state? How to handle other state? */
	}
}

static int tcp_close(struct sock *sk)
{
	struct tcp_sock *tsk = tcpsk(sk);
	/* RFC 793 Page 37 */
	switch (tsk->state) {
	case TCP_CLOSED:
		break;
	case TCP_LISTEN:
		tcp_clear_listen_queue(tsk);
		if (sk->ops->unhash)
			sk->ops->unhash(sk);
		tcp_unbhash(tsk);
		tsk->state = TCP_CLOSED;
		break;
	case TCP_SYN_RECV:
		break;
	case TCP_SYN_SENT:
		break;
	case TCP_ESTABLISHED:
		if (sk->ops->unhash)
			sk->ops->unhash(sk);
		tcp_unbhash(tsk);
		break;
	}
	return 0;
}

static struct sock_ops tcp_ops = {
//	.recv_notify = tcp_recv_notify,
//	.recv = tcp_recv_pkb,
//	.send_buf= tcp_send_buf,
//	.send_pkb = tcp_send_pkb,
	.listen = tcp_listen,
	.accept = tcp_accept,
	.connect = tcp_connect,
	.hash = tcp_hash,
	.unhash = tcp_unhash,
	.set_port = tcp_set_sport,
	.close = tcp_close,
};

struct tcp_sock *get_tcp_sock(struct tcp_sock *tsk)
{
	tsk->sk.refcnt++;
	return tsk;
}

int tcp_id;

struct sock *tcp_alloc_sock(int protocol)
{
	struct tcp_sock *tsk;
	if (protocol && protocol != IP_P_TCP)
		return NULL;
	tsk = xmalloc(sizeof(*tsk));
	memset(tsk, 0x0, sizeof(*tsk));
	tsk->sk.ops = &tcp_ops;
	tsk->state = TCP_CLOSED;
	list_init(&tsk->listen_queue);
	list_init(&tsk->accept_queue);
	list_init(&tsk->list);
	list_init(&tsk->sk.recv_queue);
	tcp_id++;
	return &tsk->sk;
}

void tcp_init(void)
{
	int i;
	for (i = 0; i < TCP_EHASH_SIZE; i++)
		hlist_head_init(&tcp_table.etable[i]);
	for (i = 0; i < TCP_LHASH_SIZE; i++)
		hlist_head_init(&tcp_table.ltable[i]);
	tcp_table.bfree = TCP_BPORT_MAX - TCP_BPORT_MIN + 1;
	/* tcp ip id */
	tcp_id = 0;
}
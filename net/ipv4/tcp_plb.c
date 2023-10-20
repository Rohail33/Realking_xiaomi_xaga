/* Protective Load Balancing (PLB)
 *
 * PLB was designed to reduce link load imbalance across datacenter
 * switches. PLB is a host-based optimization; it leverages congestion
 * signals from the transport layer to randomly change the path of the
 * connection experiencing sustained congestion. PLB prefers to repath
 * after idle periods to minimize packet reordering. It repaths by
 * changing the IPv6 Flow Label on the packets of a connection, which
 * datacenter switches include as part of ECMP/WCMP hashing.
 *
 * PLB is described in detail in:
 *
 *	Mubashir Adnan Qureshi, Yuchung Cheng, Qianwen Yin, Qiaobin Fu,
 *	Gautam Kumar, Masoud Moshref, Junhua Yan, Van Jacobson,
 *	David Wetherall,Abdul Kabbani:
 *	"PLB: Congestion Signals are Simple and Effective for
 *	 Network Load Balancing"
 *	In ACM SIGCOMM 2022, Amsterdam Netherlands.
 *
 */

#include <net/tcp.h>

/* Called once per round-trip to update PLB state for a connection. */
void tcp_plb_update_state(const struct sock *sk, struct tcp_plb_state *plb,
			  const int cong_ratio)
{
	struct net *net = sock_net(sk);

	if (!plb->enabled)
		return;

	if (cong_ratio >= 0) {
		if (cong_ratio < net->ipv4.sysctl_tcp_plb_cong_thresh)
			plb->consec_cong_rounds = 0;
		else if (plb->consec_cong_rounds <
			 net->ipv4.sysctl_tcp_plb_rehash_rounds)
			plb->consec_cong_rounds++;
	}
}
EXPORT_SYMBOL_GPL(tcp_plb_update_state);

/* Check whether recent congestion has been persistent enough to warrant
 * a load balancing decision that switches the connection to another path.
 */
void tcp_plb_check_rehash(struct sock *sk, struct tcp_plb_state *plb)
{
	struct net *net = sock_net(sk);
	bool can_idle_rehash, can_force_rehash;

	if (!plb->enabled)
		return;

	/* Note that tcp_jiffies32 can wrap, so we clear pause_until
	 * to 0 to indicate there is no recent RTO event that constrains
	 * PLB rehashing.
	 */
	if (plb->pause_until &&
	    !before(tcp_jiffies32, plb->pause_until))
		plb->pause_until = 0;

	can_idle_rehash = net->ipv4.sysctl_tcp_plb_idle_rehash_rounds &&
			  !tcp_sk(sk)->packets_out &&
			  plb->consec_cong_rounds >=
			  net->ipv4.sysctl_tcp_plb_idle_rehash_rounds;
	can_force_rehash = plb->consec_cong_rounds >=
			   net->ipv4.sysctl_tcp_plb_rehash_rounds;

	if (!plb->pause_until && (can_idle_rehash || can_force_rehash)) {
		sk_rethink_txhash(sk);
		plb->consec_cong_rounds = 0;
		tcp_sk(sk)->ecn_rehash++;
		NET_INC_STATS(sock_net(sk), LINUX_MIB_TCPECNREHASH);
	}
}
EXPORT_SYMBOL_GPL(tcp_plb_check_rehash);

/* Upon RTO, disallow load balancing for a while, to avoid having load
 * balancing decisions switch traffic to a black-holed path that was
 * previously avoided with a sk_rethink_txhash() call at RTO time.
 */
void tcp_plb_update_state_upon_rto(struct sock *sk, struct tcp_plb_state *plb)
{
	struct net *net = sock_net(sk);
	u32 pause;

	if (!plb->enabled)
		return;

	pause = net->ipv4.sysctl_tcp_plb_suspend_rto_sec * HZ;
	pause += prandom_u32_max(pause);
	plb->pause_until = tcp_jiffies32 + pause;

	/* Reset PLB state upon RTO, since an RTO causes a sk_rethink_txhash() call
	 * that may switch this connection to a path with completely different
	 * congestion characteristics.
	 */
	plb->consec_cong_rounds = 0;
}
EXPORT_SYMBOL_GPL(tcp_plb_update_state_upon_rto);

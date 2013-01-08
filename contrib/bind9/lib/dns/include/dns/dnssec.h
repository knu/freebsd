/*
 * Copyright (C) 2004-2007, 2009-2012  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2002  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id$ */

#ifndef DNS_DNSSEC_H
#define DNS_DNSSEC_H 1

/*! \file dns/dnssec.h */

#include <isc/lang.h>
#include <isc/stdtime.h>
#include <isc/stats.h>

#include <dns/diff.h>
#include <dns/types.h>

#include <dst/dst.h>

ISC_LANG_BEGINDECLS

LIBDNS_EXTERNAL_DATA extern isc_stats_t *dns_dnssec_stats;

/*%< Maximum number of keys supported in a zone. */
#define DNS_MAXZONEKEYS 32

/*
 * Indicates how the signer found this key: in the key repository, at the
 * zone apex, or specified by the user.
 */
typedef enum {
	dns_keysource_unknown,
	dns_keysource_repository,
	dns_keysource_zoneapex,
	dns_keysource_user
} dns_keysource_t;

/*
 * A DNSSEC key and hints about its intended use gleaned from metadata
 */
struct dns_dnsseckey {
	dst_key_t *key;
	isc_boolean_t hint_publish;  /*% metadata says to publish */
	isc_boolean_t force_publish; /*% publish regardless of metadata */
	isc_boolean_t hint_sign;     /*% metadata says to sign with this key */
	isc_boolean_t force_sign;    /*% sign with key regardless of metadata */
	isc_boolean_t hint_remove;   /*% metadata says *don't* publish */
	isc_boolean_t is_active;     /*% key is already active */
	isc_boolean_t first_sign;    /*% key is newly becoming active */
	unsigned int prepublish;     /*% how long until active? */
	dns_keysource_t source;      /*% how the key was found */
	isc_boolean_t ksk;           /*% this is a key-signing key */
	isc_boolean_t legacy;        /*% this is old-style key with no
					 metadata (possibly generated by
					 an older version of BIND9) and
					 should be ignored when searching
					 for keys to import into the zone */
	unsigned int index;          /*% position in list */
	ISC_LINK(dns_dnsseckey_t) link;
};

isc_result_t
dns_dnssec_keyfromrdata(dns_name_t *name, dns_rdata_t *rdata, isc_mem_t *mctx,
			dst_key_t **key);
/*%<
 *	Creates a DST key from a DNS record.  Basically a wrapper around
 *	dst_key_fromdns().
 *
 *	Requires:
 *\li		'name' is not NULL
 *\li		'rdata' is not NULL
 *\li		'mctx' is not NULL
 *\li		'key' is not NULL
 *\li		'*key' is NULL
 *
 *	Returns:
 *\li		#ISC_R_SUCCESS
 *\li		#ISC_R_NOMEMORY
 *\li		DST_R_INVALIDPUBLICKEY
 *\li		various errors from dns_name_totext
 */

isc_result_t
dns_dnssec_sign(dns_name_t *name, dns_rdataset_t *set, dst_key_t *key,
		isc_stdtime_t *inception, isc_stdtime_t *expire,
		isc_mem_t *mctx, isc_buffer_t *buffer, dns_rdata_t *sigrdata);
/*%<
 *	Generates a RRSIG record covering this rdataset.  This has no effect
 *	on existing RRSIG records.
 *
 *	Requires:
 *\li		'name' (the owner name of the record) is a valid name
 *\li		'set' is a valid rdataset
 *\li		'key' is a valid key
 *\li		'inception' is not NULL
 *\li		'expire' is not NULL
 *\li		'mctx' is not NULL
 *\li		'buffer' is not NULL
 *\li		'sigrdata' is not NULL
 *
 *	Returns:
 *\li		#ISC_R_SUCCESS
 *\li		#ISC_R_NOMEMORY
 *\li		#ISC_R_NOSPACE
 *\li		#DNS_R_INVALIDTIME - the expiration is before the inception
 *\li		#DNS_R_KEYUNAUTHORIZED - the key cannot sign this data (either
 *			it is not a zone key or its flags prevent
 *			authentication)
 *\li		DST_R_*
 */

isc_result_t
dns_dnssec_verify(dns_name_t *name, dns_rdataset_t *set, dst_key_t *key,
		  isc_boolean_t ignoretime, isc_mem_t *mctx,
		  dns_rdata_t *sigrdata);

isc_result_t
dns_dnssec_verify2(dns_name_t *name, dns_rdataset_t *set, dst_key_t *key,
		   isc_boolean_t ignoretime, isc_mem_t *mctx,
		   dns_rdata_t *sigrdata, dns_name_t *wild);
/*%<
 *	Verifies the RRSIG record covering this rdataset signed by a specific
 *	key.  This does not determine if the key's owner is authorized to sign
 *	this record, as this requires a resolver or database.
 *	If 'ignoretime' is ISC_TRUE, temporal validity will not be checked.
 *
 *	Requires:
 *\li		'name' (the owner name of the record) is a valid name
 *\li		'set' is a valid rdataset
 *\li		'key' is a valid key
 *\li		'mctx' is not NULL
 *\li		'sigrdata' is a valid rdata containing a SIG record
 *\li		'wild' if non-NULL then is a valid and has a buffer.
 *
 *	Returns:
 *\li		#ISC_R_SUCCESS
 *\li		#ISC_R_NOMEMORY
 *\li		#DNS_R_FROMWILDCARD - the signature is valid and is from
 *			a wildcard expansion.  dns_dnssec_verify2() only.
 *			'wild' contains the name of the wildcard if non-NULL.
 *\li		#DNS_R_SIGINVALID - the signature fails to verify
 *\li		#DNS_R_SIGEXPIRED - the signature has expired
 *\li		#DNS_R_SIGFUTURE - the signature's validity period has not begun
 *\li		#DNS_R_KEYUNAUTHORIZED - the key cannot sign this data (either
 *			it is not a zone key or its flags prevent
 *			authentication)
 *\li		DST_R_*
 */

/*@{*/
isc_result_t
dns_dnssec_findzonekeys(dns_db_t *db, dns_dbversion_t *ver, dns_dbnode_t *node,
			dns_name_t *name, isc_mem_t *mctx,
			unsigned int maxkeys, dst_key_t **keys,
			unsigned int *nkeys);
isc_result_t
dns_dnssec_findzonekeys2(dns_db_t *db, dns_dbversion_t *ver,
			 dns_dbnode_t *node, dns_name_t *name,
			 const char *directory, isc_mem_t *mctx,
			 unsigned int maxkeys, dst_key_t **keys,
			 unsigned int *nkeys);
/*%<
 * 	Finds a set of zone keys.
 * 	XXX temporary - this should be handled in dns_zone_t.
 */
/*@}*/

isc_result_t
dns_dnssec_signmessage(dns_message_t *msg, dst_key_t *key);
/*%<
 *	Signs a message with a SIG(0) record.  This is implicitly called by
 *	dns_message_renderend() if msg->sig0key is not NULL.
 *
 *	Requires:
 *\li		'msg' is a valid message
 *\li		'key' is a valid key that can be used for signing
 *
 *	Returns:
 *\li		#ISC_R_SUCCESS
 *\li		#ISC_R_NOMEMORY
 *\li		DST_R_*
 */

isc_result_t
dns_dnssec_verifymessage(isc_buffer_t *source, dns_message_t *msg,
			 dst_key_t *key);
/*%<
 *	Verifies a message signed by a SIG(0) record.  This is not
 *	called implicitly by dns_message_parse().  If dns_message_signer()
 *	is called before dns_dnssec_verifymessage(), it will return
 *	#DNS_R_NOTVERIFIEDYET.  dns_dnssec_verifymessage() will set
 *	the verified_sig0 flag in msg if the verify succeeds, and
 *	the sig0status field otherwise.
 *
 *	Requires:
 *\li		'source' is a valid buffer containing the unparsed message
 *\li		'msg' is a valid message
 *\li		'key' is a valid key
 *
 *	Returns:
 *\li		#ISC_R_SUCCESS
 *\li		#ISC_R_NOMEMORY
 *\li		#ISC_R_NOTFOUND - no SIG(0) was found
 *\li		#DNS_R_SIGINVALID - the SIG record is not well-formed or
 *				   was not generated by the key.
 *\li		DST_R_*
 */

isc_boolean_t
dns_dnssec_selfsigns(dns_rdata_t *rdata, dns_name_t *name,
		     dns_rdataset_t *rdataset, dns_rdataset_t *sigrdataset,
		     isc_boolean_t ignoretime, isc_mem_t *mctx);


isc_boolean_t
dns_dnssec_signs(dns_rdata_t *rdata, dns_name_t *name,
		 dns_rdataset_t *rdataset, dns_rdataset_t *sigrdataset,
		 isc_boolean_t ignoretime, isc_mem_t *mctx);
/*%<
 * Verify that 'rdataset' is validly signed in 'sigrdataset' by
 * the key in 'rdata'.
 *
 * dns_dnssec_selfsigns() requires that rdataset be a DNSKEY or KEY
 * rrset.  dns_dnssec_signs() works on any rrset.
 */


isc_result_t
dns_dnsseckey_create(isc_mem_t *mctx, dst_key_t **dstkey,
		     dns_dnsseckey_t **dkp);
/*%<
 * Create and initialize a dns_dnsseckey_t structure.
 *
 *	Requires:
 *\li		'dkp' is not NULL and '*dkp' is NULL.
 *
 *	Returns:
 *\li		#ISC_R_SUCCESS
 *\li		#ISC_R_NOMEMORY
 */

void
dns_dnsseckey_destroy(isc_mem_t *mctx, dns_dnsseckey_t **dkp);
/*%<
 * Reclaim a dns_dnsseckey_t structure.
 *
 *	Requires:
 *\li		'dkp' is not NULL and '*dkp' is not NULL.
 *
 *	Ensures:
 *\li		'*dkp' is NULL.
 */

isc_result_t
dns_dnssec_findmatchingkeys(dns_name_t *origin, const char *directory,
			    isc_mem_t *mctx, dns_dnsseckeylist_t *keylist);
/*%<
 * Search 'directory' for K* key files matching the name in 'origin'.
 * Append all such keys, along with use hints gleaned from their
 * metadata, onto 'keylist'.
 *
 *	Requires:
 *\li		'keylist' is not NULL
 *
 *	Returns:
 *\li		#ISC_R_SUCCESS
 *\li		#ISC_R_NOTFOUND
 *\li		#ISC_R_NOMEMORY
 *\li		any error returned by dns_name_totext(), isc_dir_open(), or
 *              dst_key_fromnamedfile()
 *
 *	Ensures:
 *\li		On error, keylist is unchanged
 */

isc_result_t
dns_dnssec_keylistfromrdataset(dns_name_t *origin,
			       const char *directory, isc_mem_t *mctx,
			       dns_rdataset_t *keyset, dns_rdataset_t *keysigs,
			       dns_rdataset_t *soasigs, isc_boolean_t savekeys,
			       isc_boolean_t public,
			       dns_dnsseckeylist_t *keylist);
/*%<
 * Append the contents of a DNSKEY rdataset 'keyset' to 'keylist'.
 * Omit duplicates.  If 'public' is ISC_FALSE, search 'directory' for
 * matching key files, and load the private keys that go with
 * the public ones.  If 'savekeys' is ISC_TRUE, mark the keys so
 * they will not be deleted or inactivated regardless of metadata.
 *
 * 'keysigs' and 'soasigs', if not NULL and associated, contain the
 * RRSIGS for the DNSKEY and SOA records respectively and are used to mark
 * whether a key is already active in the zone.
 */

isc_result_t
dns_dnssec_updatekeys(dns_dnsseckeylist_t *keys, dns_dnsseckeylist_t *newkeys,
		      dns_dnsseckeylist_t *removed, dns_name_t *origin,
		      dns_ttl_t ttl, dns_diff_t *diff, isc_boolean_t allzsk,
		      isc_mem_t *mctx, void (*report)(const char *, ...));
/*%<
 * Update the list of keys in 'keys' with new key information in 'newkeys'.
 *
 * For each key in 'newkeys', see if it has a match in 'keys'.
 * - If not, and if the metadata says the key should be published:
 *   add it to 'keys', and place a dns_difftuple into 'diff' so
 *   the key can be added to the DNSKEY set.  If the metadata says it
 *   should be active, set the first_sign flag.
 * - If so, and if the metadata says it should be removed:
 *   remove it from 'keys', and place a dns_difftuple into 'diff' so
 *   the key can be removed from the DNSKEY set.  if 'removed' is non-NULL,
 *   copy the key into that list; otherwise destroy it.
 * - Otherwise, make sure keys has current metadata.
 *
 * If 'allzsk' is true, we are allowing KSK-flagged keys to be used as
 * ZSKs.
 *
 * 'ttl' is the TTL of the DNSKEY RRset; if it is longer than the
 * time until a new key will be activated, then we have to delay the
 * key's activation.
 *
 * 'report' points to a function for reporting status.
 *
 * On completion, any remaining keys in 'newkeys' are freed.
 */
ISC_LANG_ENDDECLS

#endif /* DNS_DNSSEC_H */

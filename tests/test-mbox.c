#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <glib.h>
#include <time.h>

#include "gmime.h"

#define ENABLE_ZENTIMER
#include "zentimer.h"

void
print_depth (int depth)
{
	int i;
	
	for (i = 0; i < depth; i++)
		fprintf (stdout, "   ");
}

void
print_mime_struct (GMimeObject *part, int depth)
{
	const GMimeContentType *type;
	GMimeObject *object;
	
	print_depth (depth);
	type = g_mime_object_get_content_type (part);
	fprintf (stdout, "Content-Type: %s/%s\n", type->type, type->subtype);
	
	if (GMIME_IS_MULTIPART (part)) {
		GList *subpart;
		
		subpart = GMIME_MULTIPART (part)->subparts;
		while (subpart) {
			print_mime_struct (subpart->data, depth + 1);
			subpart = subpart->next;
		}
	} else if (GMIME_IS_MESSAGE_PART (part)) {
		GMimeMessagePart *mpart = (GMimeMessagePart *) part;
		
		if (mpart->message)
			print_mime_struct (mpart->message->mime_part, depth + 1);
	}
}

static void
header_cb (GMimeParser *parser, const char *header, const char *value, off_t offset, gpointer user_data)
{
	fprintf (stderr, "found \"%s:\" header at %d with a value of \"%s\"\n", header, offset, value);
}

void
test_parser (GMimeStream *stream)
{
	GMimeParser *parser;
	GMimeMessage *message;
	char *from;
	
	parser = g_mime_parser_new ();
	g_mime_parser_init_with_stream (parser, stream);
	g_mime_parser_set_scan_from (parser, TRUE);
	
	g_mime_parser_set_header_regex (parser, "^X-Evolution$", header_cb, NULL);
	
	while (!g_mime_parser_eos (parser)) {
		message = g_mime_parser_construct_message (parser);
		from = g_mime_parser_get_from (parser);
		fprintf (stdout, "%s\n", from);
		print_mime_struct (message->mime_part, 0);
		g_mime_object_unref (GMIME_OBJECT (message));
	}
	
	g_object_unref (parser);
}


/* you can only enable one of these at a time... */
/*#define STREAM_BUFFER*/
/*#define STREAM_MEM*/
/*#define STREAM_MMAP*/

int main (int argc, char **argv)
{
	char *filename = NULL;
	GMimeStream *stream, *istream;
	int fd;
	
	g_mime_init (0);
	
	if (argc > 1)
		filename = argv[1];
	else
		return 0;
	
	fd = open (filename, O_RDONLY);
	if (fd == -1)
		return 0;
	
#ifdef STREAM_MMAP
	stream = g_mime_stream_mmap_new (fd, PROT_READ, MAP_PRIVATE);
	g_assert (stream != NULL);
#else
	stream = g_mime_stream_fs_new (fd);
#endif /* STREAM_MMAP */
	
#ifdef STREAM_MEM
	istream = g_mime_stream_mem_new ();
	g_mime_stream_write_to_stream (stream, istream);
	g_mime_stream_reset (istream);
	g_mime_stream_unref (stream);
	stream = istream;
#endif
	
#ifdef STREAM_BUFFER
	istream = g_mime_stream_buffer_new (stream,
					    GMIME_STREAM_BUFFER_BLOCK_READ);
	g_mime_stream_unref (stream);
	stream = istream;
#endif
	
	test_parser (stream);
	
	g_mime_stream_unref (stream);
	
	return 0;
}

#include <config.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
extern "C" {
  #include <gmime/gmime.h>
}
#include <pan/general/debug.h>
#include <pan/general/string-view.h>
#include <pan/general/quark.h>
#include <pan/general/test.h>
#include "gnksa.h"
#include "message-check.h"

using namespace pan;

#define PRINT_ERRORS \
  if (1) { \
    int i = 0; \
    for (MessageCheck::unique_strings_t::const_iterator it(errors.begin()), end(errors.end()); it!=end; ++it, ++i) \
      std::cerr << LINE_ID << " [" << i << "][" << *it << ']' << std::endl; \
  }

int main (void)
{
  g_mime_init (0);

  MessageCheck::unique_strings_t errors;
  MessageCheck::Goodness goodness;

  quarks_t groups_our_server_has;
  groups_our_server_has.insert ("alt.test");

  // populate a simple article
  std::string attribution ("Someone wrote");
  GMimeMessage * msg = g_mime_message_new (FALSE);
  g_mime_message_set_sender (msg, "\"Charles Kerr\" <charles@rebelbase.com>");
  std::string message_id = GNKSA :: generate_message_id ("rebelbase.com");
  g_mime_message_set_message_id (msg, message_id.c_str());
  g_mime_message_set_subject (msg, "MAKE MONEY FAST");
  g_mime_message_set_header (msg, "Organization", "Lazars Android Works");
  g_mime_message_set_header (msg, "Newsgroups", "alt.test");
  GMimePart * part = g_mime_part_new_with_type ("text", "plain");
  const char * cpch = "Hello World!";
  g_mime_part_set_content (part, cpch, strlen(cpch));
  g_mime_message_set_mime_part (msg, GMIME_OBJECT(part));
  // this should pass the tests
  MessageCheck :: message_check (msg, attribution, groups_our_server_has, errors, goodness);
  check (errors.empty())
  check (goodness.is_ok())

  // all quoted
  cpch = "> Hello World!\n> All quoted text.";
  g_mime_part_set_content (part, cpch, strlen(cpch));
  MessageCheck :: message_check (msg, attribution, groups_our_server_has, errors, goodness);
  std::vector<std::string> e (errors.begin(), errors.end());
  check (errors.size() == 2)
  check (goodness.is_refuse())
  check (e[0] == "Error: Message appears to have no new content.");
  check (e[1] == "Warning: The message is entirely quoted text!");

  // mostly quoted
  cpch = "> Hello World!\n> quoted\n> text\n> foo\n> bar\nnew text";
  g_mime_part_set_content (part, cpch, strlen(cpch));
  MessageCheck :: message_check (msg, attribution, groups_our_server_has, errors, goodness);
  e.assign (errors.begin(), errors.end());
  check (errors.size() == 1)
  check (goodness.is_warn())
  check (e[0] == "Warning: The message is mostly quoted text.")

  // mostly quoted border condition: 20% of message is new content (should pass)
  cpch = "> Hello World!\n> quoted\n> text\n> foo\nnew text";
  g_mime_part_set_content (part, cpch, strlen(cpch));
  MessageCheck :: message_check (msg, attribution, groups_our_server_has, errors, goodness);
  check (errors.empty())
  check (goodness.is_ok())

  // sig check: too long
  cpch = "Hello!\n\n-- \nThis\nSig\nIs\nToo\nLong\n";
  g_mime_part_set_content (part, cpch, strlen(cpch));
  MessageCheck :: message_check (msg, attribution, groups_our_server_has, errors, goodness);
  e.assign (errors.begin(), errors.end());
  check (errors.size() == 1)
  check (goodness.is_warn())
  check (e[0] == "Warning: Signature is more than 4 lines long.");

  // sig check: too wide
  cpch = "Hello!\n"
         "\n"
         "-- \n"
         "This sig line is exactly 80 characters wide.  I'll keep typing until I reach 80.\n"
         "This sig line is greater than 80 characters wide.  In fact, it's 84 characters wide.\n"
         "This sig line is greater than 80 characters wide.  In fact, it measures 95 characters in width!\n"
         "This sig line is less than 80 characters wide.";
  g_mime_part_set_content (part, cpch, strlen(cpch));
  MessageCheck :: message_check (msg, attribution, groups_our_server_has, errors, goodness);
  e.assign (errors.begin(), errors.end());
  check (errors.size() == 1)
  check (goodness.is_warn())
  check (e[0] == "Warning: Signature is more than 80 characters wide.");

  // sig check: sig marker, no sig
  cpch = "Hello!\n\n-- \n";
  g_mime_part_set_content (part, cpch, strlen(cpch));
  MessageCheck :: message_check (msg, attribution, groups_our_server_has, errors, goodness);
  e.assign (errors.begin(), errors.end());
  check (errors.size() == 1)
  check (goodness.is_warn ())
  check (e[0] == "Warning: Signature prefix with no signature.");

  // sig check: okay sig
  cpch = "Hello!\n\n-- \nThis is a short, narrow sig.\nIt should pass.\n";
  g_mime_part_set_content (part, cpch, strlen(cpch));
  MessageCheck :: message_check (msg, attribution, groups_our_server_has, errors, goodness);
  check (errors.empty())
  check (goodness.is_ok())

  // adrian's fake followup
  cpch = ">>>>>>>>>>>> I think A\n"
         ">>>>>>>>>>> No, it's not\n"
         ">>>>>>>>>> But B => C\n"
         ">>>>>>>>> What's that got to do with A?\n"
         ">>>>>>>> I still think B => C\n"
         ">>>>>>> It's not even B => C. But Still waiting for proof for A\n"
         ">>>>>> You don't prove !A, either.\n"
         ">>>>> There's the FAQ: X => !A and Y => !A\n"
         ">>>> But there in the text it sais T' => A\n"
         ">>> But T' is only a subset of T. T => !A.\n"
         ">> Moron\n"
         "> Jackass.\n"
         "\n"
         "I don't know wether I am amused or annoyed. Apparently the funny side\n"
         "prevailed so far, as I'm still reading.\n"
         "\n"
         "-- vbi";
  MessageCheck :: message_check (msg, attribution, groups_our_server_has, errors, goodness);
  check (errors.empty())
  check (goodness.is_ok())

  // body too wide
  cpch = "Hello!\n"
         "This sig line is exactly 80 characters wide.  I'll keep typing until I reach 80.\n"
         "This sig line is greater than 80 characters wide.  In fact, it's 84 characters wide.\n"
         "This sig line is greater than 80 characters wide.  In fact, it measures 95 characters in width!\n"
         "This sig line is less than 80 characters wide.";
  g_mime_part_set_content (part, cpch, strlen(cpch));
  MessageCheck :: message_check (msg, attribution, groups_our_server_has, errors, goodness);
  e.assign (errors.begin(), errors.end());
  check (errors.size() == 1)
  check (goodness.is_warn())
  check (e[0] == "Warning: 2 lines are more than 80 characters wide.");

  // body empty
  cpch = "\n\t\n   \n-- \nThis is the sig.";
  g_mime_part_set_content (part, cpch, strlen(cpch));
  MessageCheck :: message_check (msg, attribution, groups_our_server_has, errors, goodness);
  e.assign (errors.begin(), errors.end());
  check (errors.size() == 2)
  check (goodness.is_refuse())
  check (e[0] == "Error: Message appears to have no new content.");
  check (e[1] == "Error: Message is empty.");
  cpch = "Some valid message.";
  g_mime_part_set_content (part, cpch, strlen(cpch));

  // empty subject
  g_mime_message_set_subject (msg, "");
  MessageCheck :: message_check (msg, attribution, groups_our_server_has, errors, goodness);
  e.assign (errors.begin(), errors.end());
  check (errors.size() == 1)
  check (goodness.is_refuse())
  check (e[0] == "Error: No Subject specified.");
  g_mime_message_set_subject (msg, "Happy Lucky Feeling");

  // newsgroups
  g_mime_message_set_header (msg, "Newsgroups", "alt.test,unknown.group");
  MessageCheck :: message_check (msg, attribution, groups_our_server_has, errors, goodness);
  e.assign (errors.begin(), errors.end());
  check (errors.size() == 1)
  check (goodness.is_warn())
  check (e[0] == "Warning: Unknown group \"unknown.group\".")
  g_mime_message_set_header (msg, "Newsgroups", "alt.test");

  // newsgroups
  g_mime_message_set_header (msg, "Followup-To", "alt.test,unknown.group");
  MessageCheck :: message_check (msg, attribution, groups_our_server_has, errors, goodness);
  e.assign (errors.begin(), errors.end());
  check (errors.size() == 1)
  check (goodness.is_warn ())
  check (e[0] == "Warning: Unknown group \"unknown.group\".")
  g_mime_object_remove_header (GMIME_OBJECT(msg), "Followup-To");

  // top posting
  g_mime_message_set_header (msg, "References", "<asdf@foo.com>");
  cpch = "How Fascinating!\n"
         "\n"
         "> Blah blah blah.\n";
  g_mime_part_set_content (part, cpch, strlen(cpch));
  MessageCheck :: message_check (msg, attribution, groups_our_server_has, errors, goodness);
  e.assign (errors.begin(), errors.end());
  check (errors.size() == 1)
  check (goodness.is_warn ())
  check (e[0] == "Warning: Reply seems to be top-posted.")
  g_mime_object_remove_header (GMIME_OBJECT(msg), "References");

  // top posting
  g_mime_message_set_header (msg, "References", "<asdf@foo.com>");
  cpch = "How Fascinating!\n"
         "\n"
         "> Blah blah blah.\n"
         "\n"
         "-- \n"
         "Pan shouldn't mistake this signature for\n"
         "original content in the top-posting check.\n";
  g_mime_part_set_content (part, cpch, strlen(cpch));
  MessageCheck :: message_check (msg, attribution, groups_our_server_has, errors, goodness);
  e.assign (errors.begin(), errors.end());
  check (errors.size() == 1)
  check (goodness.is_warn ())
  check (e[0] == "Warning: Reply seems to be top-posted.")
  g_mime_object_remove_header (GMIME_OBJECT(msg), "References");

  // bad signature
  cpch = "Testing to see what happens if the signature is malformed.\n"
         "It *should* warn us about it.\n"
         "\n"
         "--\n"
         "This is my signature.\n";
  g_mime_part_set_content (part, cpch, strlen(cpch));
  MessageCheck :: message_check (msg, attribution, groups_our_server_has, errors, goodness);
  e.assign (errors.begin(), errors.end());
  check (errors.size() == 1)
  check (goodness.is_warn())
  check (e[0] == "Warning: The signature marker should be \"-- \", not \"--\".");

  // success
  return 0;
}
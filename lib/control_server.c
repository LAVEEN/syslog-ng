#include "control_server.h"
#include "messages.h"
#include "misc.h"

void
control_server_init_instance(ControlServer *self, const gchar *path, GList *commands)
{
  self->control_socket_name = g_strdup(path);
  self->commands = commands;
}

void
control_connection_free(ControlConnection *self)
{
  if (self->free_fn)
    {
      self->free_fn(self);
    }
  g_string_free(self->output_buffer, TRUE);
  g_string_free(self->input_buffer, TRUE);
  g_free(self);
}

static void
control_connection_send_reply(ControlConnection *self, GString *reply)
{
  g_string_assign(self->output_buffer, reply->str);
  g_string_free(reply, TRUE);

  self->pos = 0;

  if (self->output_buffer->str[self->output_buffer->len - 1] != '\n')
    {
      g_string_append_c(self->output_buffer, '\n');
    }
  g_string_append(self->output_buffer, ".\n");

  control_connection_update_watches(self);
}

static void
control_connection_io_output(gpointer s)
{
  ControlConnection *self = (ControlConnection *) s;
  gint rc;

  rc = self->write(self, self->output_buffer->str + self->pos, self->output_buffer->len - self->pos);
  if (rc < 0)
    {
      if (errno != EAGAIN)
        {
          msg_error("Error writing control channel",
                    evt_tag_errno("error", errno),
                    NULL);
          control_connection_stop_watches(self);
          control_connection_free(self);
          return;
        }
    }
  else
    {
      self->pos += rc;
    }
  control_connection_update_watches(self);
}

static void
control_connection_io_input(void *s)
{
  ControlConnection *self = (ControlConnection *) s;
  GString *command = NULL;
  GString *reply = NULL;
  gchar *nl;
  gint rc;
  gint orig_len;
  GList *iter;

  if (self->input_buffer->len > MAX_CONTROL_LINE_LENGTH)
    {
      /* too much data in input, drop the connection */
      msg_error("Too much data in the control socket input buffer",
                NULL);
      control_connection_stop_watches(self);
      control_connection_free(self);
      return;
    }

  orig_len = self->input_buffer->len;

  /* NOTE: plus one for the terminating NUL */
  g_string_set_size(self->input_buffer, self->input_buffer->len + 128 + 1);
  rc = self->read(self, self->input_buffer->str + orig_len, 128);
  if (rc < 0)
    {
      if (errno != EAGAIN)
        {
          msg_error("Error reading command on control channel, closing control channel",
                    evt_tag_errno("error", errno),
                    NULL);
          goto destroy_connection;
        }
      /* EAGAIN, should try again when data comes */
      control_connection_update_watches(self);
      return;
    }
  else if (rc == 0)
    {
      msg_notice("EOF on control channel, closing connection",
                NULL);
      goto destroy_connection;
    }
  else
    {
      self->input_buffer->len = orig_len + rc;
      self->input_buffer->str[self->input_buffer->len] = 0;
    }

  /* here we have finished reading the input, check if there's a newline somewhere */
  nl = strchr(self->input_buffer->str, '\n');
  if (nl)
    {
      command = g_string_sized_new(128);
      /* command doesn't contain NL */
      g_string_assign_len(command, self->input_buffer->str, nl - self->input_buffer->str);
      /* strip NL */
      /*g_string_erase(self->input_buffer, 0, command->len + 1);*/
      g_string_truncate(self->input_buffer, 0);
    }
  else
    {
      /* no EOL in the input buffer, wait for more data */
      control_connection_update_watches(self);
      return;
    }

  for (iter = self->server->commands; iter != NULL; iter = iter->next)
    {
      if (strncmp(((Commands*)iter->data)->command, command->str, strlen(((Commands*)iter->data)->command)) == 0)
        {
          reply = ((Commands*)iter->data)->func(command);
          control_connection_send_reply(self, reply);
          break;
        }
    }
  if (iter == NULL)
    {
      msg_error("Unknown command read on control channel, closing control channel",
                evt_tag_str("command", command->str), NULL);
      g_string_free(command, TRUE);
      goto destroy_connection;
    }
  control_connection_update_watches(self);
  g_string_free(command, TRUE);
  return;
 destroy_connection:
  control_connection_stop_watches(self);
  control_connection_free(self);
}

void
control_connection_init_instance(ControlConnection *self, ControlServer *server)
{
  self->server = server;
  self->output_buffer = g_string_sized_new(256);
  self->input_buffer = g_string_sized_new(128);
  self->handle_input = control_connection_io_input;
  self->handle_output = control_connection_io_output;
  return;
}

void
control_server_free(ControlServer *self)
{
  if (self->free_fn)
    {
      self->free_fn(self);
    }
  g_free(self->control_socket_name);
  g_free(self);
}

#include "platforms.h"

#include <signal.h>
#include <termios.h>

static termios term_original;

auto SetBasicState() -> void
{
  struct termios term_new;
  tcgetattr( 0, &term_original );
  std::memcpy( &term_new, &term_original, sizeof( struct termios ) );
  term_new.c_lflag &= ~ECHO;
  tcsetattr( 0, TCSANOW, &term_new );

  struct sigaction sig_handler;

  sig_handler.sa_handler = &SignalHandler;

  sigemptyset( &sig_handler.sa_mask );

  sigaddset( &sig_handler.sa_mask, SIGINT );
  sigaddset( &sig_handler.sa_mask, SIGTERM );
  sigaddset( &sig_handler.sa_mask, SIGHUP );
  sigaddset( &sig_handler.sa_mask, SIGQUIT );

  sigaction( SIGINT,  &sig_handler, NULL );
  sigaction( SIGTERM, &sig_handler, NULL );
  sigaction( SIGHUP,  &sig_handler, NULL );
  sigaction( SIGQUIT, &sig_handler, NULL );
}

auto UseOldUI() -> bool
{
  return false;
}

auto SignalHandler( int signal ) -> void
{
  m_stop = true;
  tcsetattr( 0, TCSANOW, &term_original );
  return;
}

// echo imgdisp.cc | entr bash -c 'reset && g++ -ggdb3 imgdisp.cc -lX11 -lpthread -o imgdisp && echo "done."'

// https://www.lemoda.net/c/xlib-resize/
// https://stackoverflow.com/questions/54513419/putting-image-into-a-window-in-x11

#include <chrono>
#include <cstring>
#include <mutex>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <thread>
#include <X11/Xlib.h> // Every Xlib program must include this
#include <X11/extensions/shape.h>

#define READ_BUFFER_SIZE 65535

using namespace std::chrono;

typedef struct {
  uint64_t count;
  high_resolution_clock::time_point last;
} fpsCounter_s;

std::mutex image_mutex;
unsigned char *image = NULL;
uint32_t width, height;

void draw_window(Display *display, Window window, GC gc) {
  const std::lock_guard<std::mutex> lock(image_mutex);

  if (image == NULL) {
    return ;
  }

  XWindowAttributes gwa;

  XGetWindowAttributes(display, window, &gwa);
  // printf("redraw\n");

  XImage *ximage = XGetImage(display, window, 0, 0 , gwa.width, gwa.height, AllPlanes, ZPixmap);
  for (int x = 0; x < gwa.width; x++) {
    for (int y = 0; y < gwa.height ; y++) {
      uint32_t pixel =
        image[(y * width + x) * 3] << 16 |
        image[(y * width + x) * 3] << 8 |
        image[(y * width + x) * 3];
      XPutPixel(ximage, x, y, pixel);
    }
  }

  XPutImage(display, window, gc, ximage, 0, 0, 0, 0, gwa.width, gwa.height);
}

void read_image(Display *display, Window window) {
// void read_image() {
  char buf[READ_BUFFER_SIZE];
  size_t previous_size = 0;
  while (1) {
    // printf("reading image\n");

    scanf("%s", (char *) &buf);
    if (std::string(buf) != "P6") {
      fprintf(stderr, "error: wrong image type. read %s insteaf of P6\n", buf);
      return ; // fail here
    }

    scanf("%u %u", &width, &height);

    uint32_t bitdepth;
    scanf("%u", &bitdepth);

    // printf("bitdepth %u width %u height %u\n", bitdepth, width, height);
    size_t image_size = width * height * 3 + 1; // why + 1 ??

    {
      const std::lock_guard<std::mutex> lock(image_mutex);

      if (image_size != previous_size) {
        image = (unsigned char *) malloc(image_size);
        memset(image, 0, image_size);
        if (image == NULL) {
          perror("Allocation failed");
          return ;
        }
      }

      size_t read = fread(image, 1, image_size, stdin);
      if (read != image_size) {
        fprintf(stderr, "error: wrong image size. read %lu insteaf of %lu\n", read, image_size);
        return ; // fail here
      }
    }

    XClientMessageEvent event = {
      .type = ClientMessage,     /* ClientMessage */
      .serial = 0,   /* # of last request processed by server */
      .send_event = True,    /* true if this came from a SendEvent request */
      .display = display,   /* Display the event was read from */
      .window = window,
      .message_type = 0,
      .format = 8,
    };
    XSendEvent(display, window, True, 0, (XEvent *) &event);
    XFlush(display);
  }

  if (image != NULL) {
    free(image);
    image = NULL;
  }
}

void count(fpsCounter_s fpsCounter) {
  fpsCounter.count++;
  high_resolution_clock::time_point now = high_resolution_clock::now();
  double elapsed = duration_cast<duration<double>>(now - fpsCounter.last).count();
  if (elapsed > 100000.0) {
    printf("%f fps\n", fpsCounter.count / elapsed);
    fpsCounter.count = 0;
    fpsCounter.last = now;
  }
}

int loop(Display *display, Window window, GC gc) {
  fpsCounter_s fpsCounter;
  fpsCounter.last = high_resolution_clock::now();
  // Wait for the MapNotify event
  for(;;) {
    XEvent e;
    XNextEvent(display, &e);
    // printf("%d\n", e.type);
    switch (e.type) {
    case ClientMessage:
      // printf("New image\n");
      draw_window(display, window, gc);
      count(fpsCounter);
      break;
    case MapNotify:
      // draw_window(display, window);
      break;
    case ConfigureNotify:
      // printf("ConfigureNotify\n");
      // draw_window(display, window);
      break;
    }
  }
}

int main() {
  XInitThreads();
  // Init the X window system
  Display *display = XOpenDisplay(NULL);

  int blackColor = BlackPixel(display, DefaultScreen(display));
  int whiteColor = WhitePixel(display, DefaultScreen(display));

  // Create the window
  Window window = XCreateSimpleWindow(
    display, DefaultRootWindow(display), 0, 0, 200, 100, 0, whiteColor, blackColor
  );

  // We want to get MapNotify events
  XSelectInput(display, window, StructureNotifyMask);
  XMapWindow(display, window);

  GC gc = XCreateGC(display, window, 0, NULL);

  std::thread image_thread(read_image, display, window);

  loop(display, window, gc);

  // read_image();

  return 0;
}

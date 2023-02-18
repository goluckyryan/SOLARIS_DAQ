#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <png.h>
#include <fstream>
#include <cstring>
#include <vector>
#include <iostream>

void saveScreenshot(Display* display, const Window window, const char* filename){

    XWindowAttributes attrs;
    XGetWindowAttributes(display, window, &attrs);

    printf("(x,y) :(%d, %d), (w,h) : (%d, %d) \n", attrs.x, attrs.y, attrs.width, attrs.height);

    int window_width = attrs.width;
    int window_height = attrs.height;

    XImage *image = XGetImage(display, window, 0, 0, window_width, window_height, AllPlanes, ZPixmap);

    png_bytep *row_pointers = new png_bytep[window_height];
    for (int i = 0; i < window_height; i++) {
        row_pointers[i] = (png_bytep)(image->data + i * image->bytes_per_line);
    }

    FILE *png_file = fopen(filename, "w");
    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info_ptr = png_create_info_struct(png_ptr);
    png_init_io(png_ptr, png_file);
    png_set_IHDR(png_ptr, info_ptr, window_width, window_height, 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    png_write_info(png_ptr, info_ptr);
    png_write_image(png_ptr, row_pointers);
    png_write_end(png_ptr, NULL);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(png_file);

    XFree(image->data);

}

int main(int argc, char* argv[]){

    Window screenID;
    if( argc > 1){
        screenID = atoi(argv[1]);
    }

    // Connect to the X server
    Display *display = XOpenDisplay(nullptr);

    //============== show all windows ID
    Window root = DefaultRootWindow(display);

    Window parent;
    Window *children;
    unsigned int numChildren;
    Status status = XQueryTree(display, root, &root, &parent, &children, &numChildren);

    if (!status) {
        std::cerr << "Failed to query window tree\n";
        return 1;
    }

    for (unsigned int i = 0; i < numChildren; i++) {
        XTextProperty windowName;
        status = XGetWMName(display, children[i], &windowName);
        if( i == 0 ) {
            printf("%12s  | %12s \n", "Window ID", "Name");
            printf("--------------+--------------------------------\n");
        }
        if (status) {
            char **list;
            int count;
            status = XmbTextPropertyToTextList(display, &windowName, &list, &count);
            if (status >= Success && count > 0 && *list) {
                printf("%12ld  |  %s \n", children[i], *list);
                //std::cout << "Window ID: " << children[i] << ", Window Name: " << *list << "\n";
                XFreeStringList(list);
            }
            XFree(windowName.value);
        }
    }
    XFree(children);
    //================ end of show windows ID

    if( argc >1 ){
        saveScreenshot(display, screenID, "screenshot.png");
        printf("captured screenshot of windowID(%ld) as screenshot.png\n", screenID);
    }
    // Close the display connection
    XCloseDisplay(display);
    return 0;
}
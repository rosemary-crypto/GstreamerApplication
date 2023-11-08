//other compositor but freezes
#include <gst/gst.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cuda_runtime.h>
#include <infpintel/yolov5_phrd.h>
#include <opencv2/opencv.hpp>


#include <my_library.h>
#include <my_cuda.cuh>

#define DEBUGGING FALSE
#define MAX_PAD_COUNT 5

typedef struct {
    GstFormat format;
    gint64 start;
    gint64 stop;
    gint64 position;
    gboolean update;
} SegmentInfo;

SegmentInfo currentSegment = { GST_FORMAT_UNDEFINED, 0, 0, 0, FALSE };

static gboolean bus_call(GstBus* bus, GstMessage* msg, gpointer data) {
    GMainLoop* loop = (GMainLoop*)data;

    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_EOS:
        g_print("****APP: Bus End of stream\n");
        g_main_loop_quit(loop);
        break;

    case GST_MESSAGE_ERROR: {
        gchar* debug;
        GError* error;

        gst_message_parse_error(msg, &error, &debug);
        g_free(debug);

        g_printerr("****APP: Bus Error: %s\n", error->message);
        g_error_free(error);

        g_main_loop_quit(loop);
        break;
    }

    default:
        break;
    }

    return TRUE;
}

static void on_pad_added(GstElement* element, GstPad* pad, gpointer data) {
    GstPad* sinkpad;
    GstElement* melement = (GstElement*)data;

    g_print("****APP: Dynamic pad created, elements: (%s,%s), pad: %s\n", gst_element_get_name(element), gst_element_get_name(melement), GST_PAD_NAME(pad));
    sinkpad = gst_element_get_static_pad(melement, "sink");
    if (!sinkpad) {
        g_print("****APP: Sink not created successfully!");
    }
    gst_pad_link(pad, sinkpad);
    gst_object_unref(sinkpad);
}

static void on_pad_added_for_demux(GstElement* element, GstPad* pad, gpointer data) {
    GstObject* parent = gst_element_get_parent(element);
    GstElement** queues = (GstElement**)data;
    GstPad* sinkpad;

    gint pad_count = atoi(strchr(gst_pad_get_name(pad), '_') + 1);
    g_print("****APP: Pad count is : %d\n", pad_count);

    sinkpad = gst_element_get_static_pad(queues[pad_count - 1], "sink");
    if (!sinkpad) {
        g_print("****APP: Sink not created successfully!");
    }
    gst_pad_link(pad, sinkpad);
    gst_object_unref(sinkpad);
}


int main(int argc, char* argv[]) {
    GMainLoop* loop;
    GstElement* queues[MAX_PAD_COUNT];

    GstBus* bus;
    guint bus_watch_id;

    gst_init(&argc, &argv);
    loop = g_main_loop_new(NULL, FALSE);

#if DEBUGGING
    // Set the GST_DEBUG environment variable
    g_setenv("GST_DEBUG", "videomux:4", TRUE);

    // Redirect standard output and error streams to a file
    FILE* log_file;
    if (freopen_s(&log_file, "debug_ms.log", "w", stdout) != 0) {
        g_printerr("****APP: Error opening log file.\n");
        return -1;
    }

    FILE* error_log_file;
    if (freopen_s(&error_log_file, "error_debug_ms.log", "w", stderr) != 0) {
        g_printerr("****APP: Error opening error log file.\n");
        return -1;
    }
#endif

    MyLibrary trial_class;
    trial_class.doSomething();
    cu_function();
    //cu_function();

        // Initialize a Fibonacci relation sequence.
    //fibonacci_init(1, 1);
    //// Write out the sequence values until overflow.
    //do {
    //    std::cout << fibonacci_index() << ": "
    //        << fibonacci_current() << std::endl;
    //} while (fibonacci_next());
    //// Report count of values written before overflow.
    //std::cout << fibonacci_index() + 1 <<
    //    " Fibonacci sequence values fit in an " <<
    //    "unsigned 64-bit integer." << std::endl;

    GstElement* pipeline = gst_pipeline_new("video-player");
    GstElement* source1 = gst_element_factory_make("filesrc", "file-source1");
    GstElement* source2 = gst_element_factory_make("filesrc", "file-source2");
    GstElement* decoder1 = gst_element_factory_make("decodebin", "decode-bin1");
    GstElement* decoder2 = gst_element_factory_make("decodebin", "decode-bin2");
    GstElement* muxer = gst_element_factory_make("videomux", "muxer");
    GstElement* demuxer = gst_element_factory_make("videodemux", "custom-demuxer");
    GstElement* sink = gst_element_factory_make("autovideosink", "video-output1");
    GstElement* sink2 = gst_element_factory_make("autovideosink", "video-output2");
    GstElement* videoscale1 = gst_element_factory_make("videoscale", "videoscale1");
    GstElement* videoscale2 = gst_element_factory_make("videoscale", "videoscale2");
    GstElement* compositor = gst_element_factory_make("compositor", "my-compositor");

    GstPad* compositor_pad1 = gst_element_request_pad(compositor,
        gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(compositor), "sink_%u"),
        NULL, NULL);
    GstPad* compositor_pad2 = gst_element_request_pad(compositor,
        gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(compositor), "sink_%u"),
        NULL, NULL);

    GstCaps* caps = gst_caps_new_simple("video/x-raw",
        "width", G_TYPE_INT, 640,
        "height", G_TYPE_INT, 360,
        NULL);
    GstElement* capsfilter1 = gst_element_factory_make("capsfilter", "capsfilter1");
    g_object_set(capsfilter1, "caps", caps, NULL);
    GstElement* capsfilter2 = gst_element_factory_make("capsfilter", "capsfilter2");
    g_object_set(capsfilter2, "caps", caps, NULL);

    for (int i = 0; i < MAX_PAD_COUNT; i++) {
        gchar* queue_name = g_strdup_printf("queue_%d", i + 1);
        queues[i] = gst_element_factory_make("queue", queue_name);
        g_free(queue_name);

        if (!queues[i]) {
            g_printerr("****APP: Queue element could not be created. Exiting.\n");
            return -1;
        }
    }

    g_object_set(G_OBJECT(source1), "location", "D:/videos/vid1.mp4", NULL);
    g_object_set(G_OBJECT(source2), "location", "D:/videos/vid3.mp4", NULL);
    g_object_set(G_OBJECT(compositor_pad1), "xpos", 0, "ypos", 0, NULL);
    g_object_set(G_OBJECT(compositor_pad2), "xpos", 640, "ypos", 360, NULL);

    bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    bus_watch_id = gst_bus_add_watch(bus, bus_call, loop);
    gst_object_unref(bus);

    gst_bin_add_many(GST_BIN(pipeline),
        source1, decoder1, videoscale1, capsfilter1, muxer, demuxer,
        source2, decoder2, videoscale2, capsfilter2,
        queues[0], queues[1],
        compositor, sink, NULL);

    gst_element_link(source1, decoder1);
    gst_element_link(source2, decoder2);
    g_signal_connect(decoder1, "pad-added", G_CALLBACK(on_pad_added), videoscale1);
    g_signal_connect(decoder2, "pad-added", G_CALLBACK(on_pad_added), videoscale2);
    gst_element_link_many(videoscale1, capsfilter1, muxer, NULL);
    gst_element_link_many(videoscale2, capsfilter2, muxer, NULL);
    gst_element_link(muxer, demuxer);
    g_signal_connect(demuxer, "pad-added", G_CALLBACK(on_pad_added_for_demux), queues);
    gst_element_link(queues[0], compositor);
    gst_element_link(queues[1], compositor);
    gst_element_link(compositor, sink);

    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    g_main_loop_run(loop);

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(pipeline));
    g_source_remove(bus_watch_id);
    g_main_loop_unref(loop);
    gst_caps_unref(caps);

#if DEBUGGING
    fclose(stderr);
    fclose(stdout);
#endif
    return 0;
}
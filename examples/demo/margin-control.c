#include "gtk-layer-demo.h"

static void
on_orientation_changed (GtkWindow *_window, WindowOrientation orientation, GtkWidget *box)
{
    (void)_window;

    switch (orientation) {
        case WINDOW_ORIENTATION_HORIZONTAL:
            gtk_orientable_set_orientation (GTK_ORIENTABLE (box), GTK_ORIENTATION_HORIZONTAL);
            break;
        case WINDOW_ORIENTATION_VERTICAL:
            gtk_orientable_set_orientation (GTK_ORIENTABLE (box), GTK_ORIENTATION_VERTICAL);
            break;
        case WINDOW_ORIENTATION_NONE:
            gtk_orientable_set_orientation (GTK_ORIENTABLE (box), GTK_ORIENTATION_VERTICAL);
            break;
    }
}

typedef struct {
    GtkLayerShellEdge edge;
    GtkWindow *layer_window;
} MarginSpinButtonData;

static void
on_margin_changed (GtkSpinButton *button, MarginSpinButtonData *data)
{
    int value = gtk_spin_button_get_value (button);
    gtk_layer_set_margin (data->layer_window, data->edge, value);
}

static GtkWidget *
margin_spin_button_new (GtkWindow *layer_window,
                        GtkLayerShellEdge edge,
                        const char *tooltip,
                        const int default_margins[GTK_LAYER_SHELL_EDGE_ENTRY_NUMBER])
{
    GtkAdjustment *adjustment = gtk_adjustment_new (default_margins[edge], 0.0, 4000.0, 20.0, 50.0, 0.0);
    GtkWidget *button = gtk_spin_button_new (adjustment, 0.5, 0);
    gtk_widget_set_tooltip_text (button, tooltip);
    MarginSpinButtonData *data = g_new0 (MarginSpinButtonData, 1);
    *data = (MarginSpinButtonData) {
        .edge = edge,
        .layer_window = layer_window,
    };
    g_object_set_data_full(G_OBJECT (button), "value-changed_signal_data", data, (GDestroyNotify)g_free);
    g_signal_connect (button, "value-changed", G_CALLBACK (on_margin_changed), data);
    return button;
}

static void
on_open_clicked (GtkWidget *_button, GtkWidget *popover)
{
    (void)_button;

    gtk_popover_popup (GTK_POPOVER (popover));
}

GtkWidget *
margin_control_new (GtkWindow *layer_window, const int default_margins[GTK_LAYER_SHELL_EDGE_ENTRY_NUMBER])
{
    const int spacing = 4;
    GtkWidget *switch_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, spacing);
    g_signal_connect (layer_window, "orientation-changed", G_CALLBACK (on_orientation_changed), switch_box);
    {
        GtkWidget *vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, spacing);
        gtk_box_append (GTK_BOX (switch_box), vbox);
        {
            GtkWidget *button = margin_spin_button_new (layer_window, GTK_LAYER_SHELL_EDGE_LEFT, "Left margin", default_margins);
            gtk_box_append (GTK_BOX (vbox), button);
        }
        {
            GtkWidget *button = margin_spin_button_new (layer_window, GTK_LAYER_SHELL_EDGE_RIGHT, "Right margin", default_margins);
            gtk_box_append (GTK_BOX (vbox), button);
        }
    }
    {
        GtkWidget *vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, spacing);
        gtk_box_append (GTK_BOX (switch_box), vbox);
        {
            GtkWidget *button = margin_spin_button_new (layer_window, GTK_LAYER_SHELL_EDGE_TOP, "Top margin", default_margins);
            gtk_box_append (GTK_BOX (vbox), button);
        }
        {
            GtkWidget *button = margin_spin_button_new (layer_window, GTK_LAYER_SHELL_EDGE_BOTTOM, "Bottom margin", default_margins);
            gtk_box_append (GTK_BOX (vbox), button);
        }
    }

    GtkWidget *open_button = gtk_button_new_with_label ("Set margin");
    gtk_widget_set_tooltip_text (open_button, "Space to leave empty around surface");
    GtkWidget *popover = gtk_popover_new ();
    gtk_popover_set_default_widget (GTK_POPOVER (popover), open_button);
    gtk_popover_set_autohide (GTK_POPOVER (popover), TRUE);
    gtk_popover_set_position (GTK_POPOVER (popover), GTK_POS_BOTTOM);
    gtk_popover_set_child (GTK_POPOVER (popover), switch_box);
    gtk_widget_set_visible (switch_box, TRUE);
    g_signal_connect (open_button, "clicked", G_CALLBACK (on_open_clicked), popover);
    return open_button;
}

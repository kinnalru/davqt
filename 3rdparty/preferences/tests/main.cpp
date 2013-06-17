
#include <list>
#include <boost/bind.hpp>

#include "preferences_dialog.h"
#include "preferences_widget.h"

#include <QApplication>
#include <QIcon>

typedef std::list<preferences_dialog*> Dialogs;

int main(int c, char* v[])
{
    QApplication app(c, v);

    Dialogs d;
    d.push_back( new preferences_dialog(preferences_dialog::Tree, false, 0)  );
    d.back()->setWindowTitle("Custom Tree");

    d.push_back( new preferences_dialog(preferences_dialog::List, false, 0)  );
    d.back()->setWindowTitle("Custom List");

    d.push_back( new preferences_dialog(preferences_dialog::Tab, false, 0)  );
    d.back()->setWindowTitle("Custom Tab");

    d.push_back( new preferences_dialog(preferences_dialog::Tree, true, 0)  );
    d.back()->setWindowTitle("Native Tree");

    d.push_back( new preferences_dialog(preferences_dialog::List, true, 0)  );
    d.back()->setWindowTitle("Native List");

    d.push_back( new preferences_dialog(preferences_dialog::Tab, true, 0)  );
    d.back()->setWindowTitle("Native Tab");


   for( Dialogs::iterator it = d.begin(); it != d.end(); ++it  )
   {
   	preferences_widget* w = new preferences_widget(0, "Widget1");
	w->set_icon( QIcon("/usr/share/icons/oxygen/32x32/actions/checkbox.png") );
   	preferences_item item = (*it)->add_item( w );

	try{ 
	    (*it)->add_item( new preferences_widget(0, "SUB Widget1"), item );
	}
	catch(...){}

   	(*it)->add_item( new preferences_widget(0, "Widget2") );

	(*it)->show();
   }


    return app.exec();
}



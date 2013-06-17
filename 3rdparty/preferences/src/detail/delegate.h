
#ifndef LIB_PREF_DELEGATE_H
#define LIB_PREF_DELEGATE_H

#include <QApplication>
#include <QAbstractItemDelegate>
#include <QTextLayout>
#include <QPainter>

inline int layoutText( QTextLayout *layout, int maxWidth )
{
    qreal height = 0;
    int textWidth = 0;
    layout->beginLayout();
    while (true) {
        QTextLine line = layout->createLine();
        if (!line.isValid()) {
            break;
        }
        line.setLineWidth(maxWidth);
        line.setPosition(QPointF(0, height));
        height += line.height();
        textWidth = qMax(textWidth, qRound(line.naturalTextWidth() + 0.5));
    }
    layout->endLayout();
    return textWidth;
}


class KLikeListViewDelegate : public QAbstractItemDelegate
{
  Q_OBJECT

  public:
    KLikeListViewDelegate( QObject *parent = 0 )
        : QAbstractItemDelegate( parent )
    {}


    virtual void paint( QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index ) const
    {
        if ( !index.isValid() )
            return;

        const QString text = index.model()->data( index, Qt::DisplayRole ).toString();
        const QIcon icon = index.model()->data( index, Qt::DecorationRole ).value<QIcon>();
        const QPixmap pixmap = icon.pixmap( 32,32 );

        QFontMetrics fm = painter->fontMetrics();
        int wp = pixmap.width();
        int hp = pixmap.height();

        QTextLayout iconTextLayout( text, option.font );
        QTextOption textOption( Qt::AlignHCenter );
        iconTextLayout.setTextOption( textOption );
        int maxWidth = qMax( 3 * wp, 8 * fm.height() );
        layoutText( &iconTextLayout, maxWidth );

        QPen pen = painter->pen();
        QPalette::ColorGroup cg = option.state & QStyle::State_Enabled
                                    ? QPalette::Normal : QPalette::Disabled;
        if ( cg == QPalette::Normal && !(option.state & QStyle::State_Active) )
            cg = QPalette::Inactive;

        QStyleOptionViewItemV4 opt(option);
        opt.showDecorationSelected = true;
        QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);
        if ( option.state & QStyle::State_Selected ) {
            painter->setPen( option.palette.color( cg, QPalette::HighlightedText ) );
        } else {
            painter->setPen( option.palette.color( cg, QPalette::Text ) );
        }

        painter->drawPixmap( option.rect.x() + (option.rect.width()/2)-(wp/2), option.rect.y() + 5, pixmap );
        if ( !text.isEmpty() )
            iconTextLayout.draw( painter, QPoint( option.rect.x() + (option.rect.width()/2)-(maxWidth/2), option.rect.y() + hp+7 ) );

        painter->setPen( pen );

        drawFocus( painter, option, option.rect );
    }

    virtual QSize sizeHint( const QStyleOptionViewItem &option, const QModelIndex &index ) const
    {
        if ( !index.isValid() )
            return QSize( 0, 0 );

        const QString text = index.model()->data( index, Qt::DisplayRole ).toString();
        const QIcon icon = index.model()->data( index, Qt::DecorationRole ).value<QIcon>();
        const QPixmap pixmap = icon.pixmap( 32, 32 );

        QFontMetrics fm = option.fontMetrics;
        int gap = fm.height();
        int wp = pixmap.width();
        int hp = pixmap.height();

        if ( hp == 0 ) {
            /**
            * No pixmap loaded yet, we'll use the default icon size in this case.
            */
            hp = 32;
            wp = 32;
        }

        QTextLayout iconTextLayout( text, option.font );
        int wt = layoutText( &iconTextLayout, qMax( 3 * wp, 8 * fm.height() ) );
        int ht = iconTextLayout.boundingRect().height();

        int width, height;
        if ( text.isEmpty() )
            height = hp;
        else
            height = hp + ht + 10;

        width = qMax( wt, wp ) + gap;

        return QSize( width, height );
    }



    private:
    void drawFocus( QPainter* painter, const QStyleOptionViewItem& option, const QRect& rect ) const
    {
        if (option.state & QStyle::State_HasFocus) {
            QStyleOptionFocusRect o;
            o.QStyleOption::operator=(option);
            o.rect = rect;
            o.state |= QStyle::State_KeyboardFocusChange;
            QPalette::ColorGroup cg = (option.state & QStyle::State_Enabled)
                                    ? QPalette::Normal : QPalette::Disabled;
            o.backgroundColor = option.palette.color( cg, (option.state & QStyle::State_Selected)
                                                    ? QPalette::Highlight : QPalette::Background );
            QApplication::style()->drawPrimitive( QStyle::PE_FrameFocusRect, &o, painter );
        }
    }

};


#endif


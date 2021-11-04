#include <qitemdelegate.h>

class DefaultItemDelegate : public QItemDelegate
{
    Q_OBJECT
public:
    DefaultItemDelegate()
    {
    }
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        QStyleOptionViewItem  viewOption(option);

        const QColor hlClr = Qt::white;
        const QColor txtClr = Qt::black;

        viewOption.palette.setColor(QPalette::Highlight, hlClr);
        viewOption.palette.setColor(QPalette::HighlightedText, txtClr);

        QItemDelegate::paint(painter, viewOption, index);
    }
};

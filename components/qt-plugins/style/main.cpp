#include <QStylePlugin>

#include "bayustyle.h"

class BayuStylePlugin : public QStylePlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QStyleFactoryInterface_iid FILE "bayu-style.json")

public:
    QStyle *create(const QString &key) override
    {
        if (key.compare(QLatin1String("bayu"), Qt::CaseInsensitive) == 0)
            return new BayuStyle;
        return nullptr;
    }
};

#include "main.moc"

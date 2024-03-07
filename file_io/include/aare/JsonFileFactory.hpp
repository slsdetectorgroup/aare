#include "aare/FileFactory.hpp"
template <DetectorType detector,typename DataType>
class JsonFileFactory: public FileFactory<detector,DataType>
{
private:
    /* data */
public:
    File<detector,DataType>* load_file() override;
    void parse_metadata(File<detector,DataType>*) override;
    JsonFileFactory<detector,DataType>(std::filesystem::path fpath);
    void open_subfiles(File<detector,DataType>*);
    


};


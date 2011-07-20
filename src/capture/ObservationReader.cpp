#include <ecto/ecto.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <objcog/db/couch.hpp>
#include <objcog/db/opencv.h>
#include <boost/python/stl_iterator.hpp>
#include <boost/format.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <string>
#include <objcog/capture/capture.hpp>

using ecto::tendrils;
namespace objcog
{
  namespace capture
  {
#define STRINGYFY(A) #A

    std::string where_doc_id = STRINGYFY(
        function(doc)
        {
          if(doc.object_id == "%s" )
          emit("frame_number",doc.frame_number);
        }
    );
    struct result_less
    {
      bool operator()(const couch::View::result& lhs, const couch::View::result& rhs)
      {
        return boost::lexical_cast<int>(lhs.value) < boost::lexical_cast<int>(rhs.value);
      }
    };

    struct ObservationReader
    {
      static void
      declare_params(tendrils& params)
      {
        params.declare<std::string>("object_id", "The object id, to associate this frame with.", "object_01");
        params.declare<std::string>("db_url", "The database url", std::string(DEFAULT_COUCHDB_URL));
      }

      static void
      declare_io(const tendrils& params, const tendrils& inputs, tendrils& outputs)
      {
        outputs.declare<cv::Mat>("image", "An rgb full frame image.");
        outputs.declare<cv::Mat>("depth", "The 16bit depth image.");
        outputs.declare<cv::Mat>("mask", "The mask.");
        outputs.declare<cv::Mat>("R", "The orientation.");
        outputs.declare<cv::Mat>("T", "The translation.");
        outputs.declare<cv::Mat>("K", "The camera intrinsic matrix");
        outputs.declare<int>("frame_number", "The frame number");
      }
      void
      on_object_id_change(const std::string& id)
      {
        couch::View v;
        v.add_map("map", boost::str(boost::format(where_doc_id) % id));
        std::vector<couch::View::result> results = db.run_view(v, -1, 0, total_rows, offset);
        std::sort(results.begin(),results.end(),result_less());
        BOOST_FOREACH(const couch::View::result& x, results)
             {
               couch::Document doc(db, x.id);
               docs.push_back(doc);
             }
        current_frame = 0;
      }
      ObservationReader()
          :
            current_frame(0)
      {
      }
      void
      configure(tendrils& params, tendrils& inputs, tendrils& outputs)
      {
        db = couch::Db(params.get<std::string>("db_url")+ "/observations");
        db.update_info();
        ecto::spore<std::string> object_id = params.at("object_id");
        object_id.set_callback(boost::bind(&ObservationReader::on_object_id_change, this, _1));
      }
      int
      process(const tendrils& inputs, tendrils& outputs)
      {
        couch::Document doc = docs[current_frame];
        doc.update();
        obs << doc; //read the observation from the doc.
        obs >> outputs; //push the observation to the outputs.
        current_frame++;
        if (current_frame >= int(docs.size()))
          return ecto::QUIT;
        return 0;
      }
      std::vector<couch::Document> docs;
      Observation obs;
      int total_rows, offset;
      couch::Db db;
      int current_frame;
    };
  }
}
ECTO_CELL(capture,objcog::capture::ObservationReader,"ObservationReader","Reads observations from the database.");
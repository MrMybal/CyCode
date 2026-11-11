#include "cycode/text/Document.h"
#include "cycode/text/Encoding.h"
#include "cycode/text/DocumentState.h"
#include "cycode/text/DocumentSession.h"
#include "cycode/Paths.h"
#include <fstream>
#include <stdexcept>
using namespace cycode;
using namespace cycode::text;
namespace {
void require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
template<class F>void refuses(F f){bool failed=false;try{f();}catch(...){failed=true;}require(failed,"Expected native document refusal");}
}
namespace cycode {
void nativeDocumentContracts(const std::filesystem::path &base) {
    const auto root=base/"native-text-documents";std::filesystem::create_directories(root);
        const auto file=root/utf8Path("code é.cpp");
        const std::string original="\xef\xbb\xbf\tint value = 1;\r\n// fin\n";
        writeTextFileAtomic(file,original,std::nullopt);
        auto doc=Document::open(file);
        require(!doc.dirty() && doc.text()=="\tint value = 1;\n// fin\n","Decode BOM/newline/tab");
        doc.save(); require(readTextFileBytes(file)==original,"No-op save changed original bytes");
        auto copy=root/"copy.cpp";
        doc.saveAs(copy,std::nullopt); require(readTextFileBytes(copy)==original,"No-op Save As lost mixed endings");
        doc.replace("\tint value = 2;\n"); doc.save();
        require(readTextFileBytes(copy)=="\xef\xbb\xbf\tint value = 2;\r\n","Save encoding/newlines");
        doc.replace("edited\n"); writeTextFileAtomic(copy,"external\n",readTextFileBytes(copy));
        refuses([&] { doc.save(); }); require(doc.dirty() && doc.text()=="edited\n","Conflict lost buffer");
        refuses([&] { doc.saveAs(copy,std::string("external\n")); });
        require(readTextFileBytes(copy)=="external\n","Save As bypassed same-file conflict");
        Document untitled; untitled.replace("new\n");
        refuses([&] { untitled.saveAs(copy,std::nullopt); });
        refuses([&] { untitled.saveAs(copy,std::string("outdated")); });
        require(untitled.untitled() && untitled.dirty(),"Failed Save As changed identity");
        untitled.saveAs(copy,std::string("external\n")); require(!untitled.dirty(),"Save As remains dirty");
        require(readTextFileBytes(copy)=="new\n","Explicit overwrite failed");
        const auto cr=root/"mac.txt"; writeTextFileAtomic(cr,"a\rb\r",std::nullopt);
        auto oldMac=Document::open(cr); oldMac.replace("c\nd\n"); oldMac.save();
        require(readTextFileBytes(cr)=="c\rd\r","CR newlines lost");
        const auto bad=root/"binary.bin"; writeTextFileAtomic(bad,std::string("a\0b",3),std::nullopt);
        refuses([&] { Document::open(bad); });
        refuses([&] { untitled.replace(std::string("\xc0\xaf",2)); });
        refuses([&] { untitled.replace(std::string(maxTextFileBytes+1,'x')); });
        std::filesystem::remove(copy); refuses([&] { untitled.save(); });
        require(untitled.text()=="new\n","Missing disk lost buffer");
        for (const auto &mode : {"utf-8","utf-8-bom","utf-16le","utf-16be"}) {
            const auto encoded=encodeText("été 🛠\r\n",mode);require(decodeText(encoded).text=="été 🛠\r\n","Encoding round trip");
            const auto encodedFile=root/(std::string(mode)+".txt");writeTextFileAtomic(encodedFile,encoded,std::nullopt);auto converted=Document::open(encodedFile);
            converted.save();require(readTextFileBytes(encodedFile)==encoded,"No-op encoding save changed disk");converted.format("utf-16be","LF");require(converted.dirty(),"Encoding-only change is not dirty");converted.save();require(decodeText(readTextFileBytes(encodedFile)).text=="été 🛠\n","Pending encoding/newline conversion not saved");
        }
        require(decodeText(std::string("\x80\xe9",2),"windows-1252").text=="€é","Windows-1252 decode");
        require(encodeText("€é","windows-1252")==std::string("\x80\xe9",2),"Windows-1252 encode");
        refuses([&] { encodeText("🛠","windows-1252"); });
        refuses([&] { decodeText(std::string("\xff\xfe\x00\xd8",4)); });
        refuses([&] { decodeText(std::string("\xfe\xff\xdc\x00",4)); });
        Document formatGuard;formatGuard.replace("🛠");refuses([&] { formatGuard.format("windows-1252","LF"); });require(std::string(formatGuard.encoding())=="UTF-8","Failed encoding conversion mutated mode");
        for (const auto &entry : std::filesystem::directory_iterator(root)) require(entry.path().filename().string().find(".cycode-save-")==std::string::npos,"Temporary file leaked");

    for(const auto *mode:{"utf-8","utf-8-bom","utf-16le","utf-16be"}) {
        const auto raw=text::encodeText("été 🛠\r\n",mode);
        auto buffer=text::decodeBuffer(raw,mode);
        require(!text::bufferHasChanges(buffer) && text::encodeBuffer(buffer)==raw,"Canonical buffer encoding round trip");
        buffer.identity=77;buffer.revision=99;
        text::replaceBuffer(buffer,"edited 🛠\n");
        require(buffer.dirty && buffer.identity==77 && buffer.revision==99,"Buffer primitive changed owner-issued identity/revision");
        const auto previousEncoding=buffer.encoding;
        refuses([&]{text::formatBuffer(buffer,"windows-1252","LF");});
        require(buffer.encoding==previousEncoding && buffer.text=="edited 🛠\n","Failed encoding conversion mutated canonical buffer");
        text::markBufferSaved(buffer,text::encodeBuffer(buffer));
        require(!buffer.dirty && !text::bufferHasChanges(buffer),"Saved buffer baseline incoherent");
    }
    text::DocumentSession session;
    const auto opened=session.dispatch("documents.open",{{"path",pathUtf8(file)}});
    const auto changed=session.dispatch("documents.replace",{{"id",opened.at("id")},{"expectedRevision",opened.at("revision")},{"text","needle 🛠\n"}});
    require(changed.at("dirty")==true && readTextFileBytes(file)==original,"Session saved an unsaved edit");
    refuses([&]{session.dispatch("documents.replace",{{"id",opened.at("id")},{"expectedRevision",opened.at("revision")},{"text","stale"}});});
    const auto matches=session.dispatch("search.openDocuments",{{"query","needle"}});
    require(matches.at("matches").size()==1 && matches.at("matches")[0].at("revision")==changed.at("revision"),"Search did not use canonical live document revision");
    auto live=session.documents().at(opened.at("id").get<size_t>());live->readOnly=true;
    refuses([&]{session.dispatch("documents.replace",{{"id",opened.at("id")},{"expectedRevision",changed.at("revision")},{"text","forbidden"}});});
    live->readOnly=false;
    session.dispatch("documents.close",{{"id",opened.at("id")},{"expectedRevision",changed.at("revision")},{"discard",true}});
    require(session.dispatch("documents.list").at("documents").empty(),"Closed native session document remained live");
}
} // namespace cycode

// -*- c-basic-offset: 2 -*-

#include "Flatbuffers.h"

std::string to_string(const Date &date) {
  uint64_t seconds_per_day = 60 * 60 * 24L;
  uint64_t secs = date.days_since_epoch * seconds_per_day;
  struct tm tm;
  secs_to_tm(secs, &tm);
  char buffer[80];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d", &tm);
  return std::string(buffer);
}

void print(const tuix::Row *in) {
  flatbuffers::uoffset_t num_fields = in->field_values()->size();
  printf("[");
  for (flatbuffers::uoffset_t i = 0; i < num_fields; i++) {
    print(in->field_values()->Get(i));
    if (i + 1 < num_fields) {
      printf(",");
    }
  }
  printf("]\n");
}

void print(const tuix::Field *field) {
  if (field->is_null()) {
    printf("null");
  } else {
    switch (field->value_type()) {
    case tuix::FieldUnion_BooleanField:
      printf("%s",
             static_cast<const tuix::BooleanField *>(field->value())->value() ? "true" : "false");
      break;
    case tuix::FieldUnion_IntegerField:
      printf("%d", static_cast<const tuix::IntegerField *>(field->value())->value());
      break;
    case tuix::FieldUnion_LongField:
      printf("%ld", static_cast<const tuix::LongField *>(field->value())->value());
      break;
    case tuix::FieldUnion_FloatField:
      printf("%f", static_cast<const tuix::FloatField *>(field->value())->value());
      break;
    case tuix::FieldUnion_DoubleField:
      printf("%lf", static_cast<const tuix::DoubleField *>(field->value())->value());
      break;
    case tuix::FieldUnion_StringField:
    {
      auto string_field = static_cast<const tuix::StringField *>(field->value());
      printf("%s",
             std::string(reinterpret_cast<const char *>(string_field->value()->data()),
                         string_field->length()).c_str());
      break;
    }
    case tuix::FieldUnion_DateField:
      printf("%s", to_string(Date(static_cast<const tuix::DateField *>(field->value())->value()))
             .c_str());
      break;
    default:
      printf("print(tuix::Row *): Unknown field type %d\n",
             field->value_type());
      std::exit(1);
    }
  }

}

void extract_dataset(EncryptedBlocksToRowReader &r, double* features, double* labels, int &sample_num, int &attribute_num) {
  int feature_ptr = 0;
  int label_ptr = 0;
  sample_num = 0;
  while(r.has_next()) {
    ++sample_num;
    const tuix::Row *row = r.next();
    attribute_num = row->field_values()->Length()-1;
    for(int i = 0; i <= attribute_num; ++i) {
      double value = static_cast<const tuix::DoubleField*>(row->field_values()->Get(i)->value())->value();
      if(i != attribute_num)
        features[feature_ptr++] = value;
      else
        labels[label_ptr++] = value;
    }
  }
}

void serialize_dataset(flatbuffers::FlatBufferBuilder &builder, FlatbuffersRowWriter &w, double* features, double* labels, int sample_num, int attribute_num) {
  for(int i = 0; i < sample_num; ++i) {
    std::vector<flatbuffers::Offset<tuix::Field>> tmp_row;
    for(int j = 0; j < attribute_num; ++j) {
      tmp_row.push_back(tuix::CreateField(builder, tuix::FieldUnion_DoubleField, tuix::CreateDoubleField(builder, features[i*attribute_num+i+j]).Union()));
    }
    tmp_row.push_back(tuix::CreateField(builder, tuix::FieldUnion_DoubleField, tuix::CreateDoubleField(builder, labels[i]).Union()));
    const tuix::Row *final_row = flatbuffers::GetTemporaryPointer<tuix::Row>(builder, tuix::CreateRow(builder, builder.CreateVector(tmp_row)));
    w.write(final_row);
  } 
}

void extract_vector(EncryptedBlocksToRowReader &r, double* theta, int &sample_num, int &attribute_num) {
  int theta_ptr = 0;
  while(r.has_next()) {
    ++sample_num;
    const tuix::Row *row = r.next();
    attribute_num = row->field_values()->Length();
    for(int i = 0; i < attribute_num; ++i) {
      double value = static_cast<const tuix::DoubleField*>(row->field_values()->Get(i)->value())->value();
      theta[theta_ptr++] = value;
    }
  }
}

void serialize_vector(flatbuffers::FlatBufferBuilder &builder, FlatbuffersRowWriter &w, double* theta, int sample_num, int attribute_num) {
  for(int i = 0; i < sample_num; ++i) {
    std::vector<flatbuffers::Offset<tuix::Field> > tmp_row;
    for(int j = 0; j < attribute_num; ++j) {
      tmp_row.push_back(tuix::CreateField(builder, tuix::FieldUnion_DoubleField, tuix::CreateDoubleField(builder, theta[j]).Union()));
  }
    tmp_row = tmp_row;
    const tuix::Row *final_row = flatbuffers::GetTemporaryPointer<tuix::Row>(builder, tuix::CreateRow(builder, builder.CreateVector(tmp_row)));
    w.write(final_row);
  }
}

template<>
flatbuffers::Offset<tuix::Row> flatbuffers_copy(
  const tuix::Row *row, flatbuffers::FlatBufferBuilder& builder, bool force_null) {

  flatbuffers::uoffset_t num_fields = row->field_values()->size();
  std::vector<flatbuffers::Offset<tuix::Field>> field_values(num_fields);
  for (flatbuffers::uoffset_t i = 0; i < num_fields; i++) {
    field_values[i] = flatbuffers_copy<tuix::Field>(
      row->field_values()->Get(i), builder, force_null);
  }
  return tuix::CreateRowDirect(builder, &field_values);
}

template<>
flatbuffers::Offset<tuix::Field> flatbuffers_copy(
  const tuix::Field *field, flatbuffers::FlatBufferBuilder& builder, bool force_null) {

  bool is_null = force_null || field->is_null();
  switch (field->value_type()) {
  case tuix::FieldUnion_BooleanField:
    return tuix::CreateField(
      builder,
      tuix::FieldUnion_BooleanField,
      tuix::CreateBooleanField(
        builder,
        static_cast<const tuix::BooleanField *>(field->value())->value()).Union(),
      is_null);
  case tuix::FieldUnion_IntegerField:
    return tuix::CreateField(
      builder,
      tuix::FieldUnion_IntegerField,
      tuix::CreateIntegerField(
        builder,
        static_cast<const tuix::IntegerField *>(field->value())->value()).Union(),
      is_null);
  case tuix::FieldUnion_LongField:
    return tuix::CreateField(
      builder,
      tuix::FieldUnion_LongField,
      tuix::CreateLongField(
        builder,
        static_cast<const tuix::LongField *>(field->value())->value()).Union(),
      is_null);
  case tuix::FieldUnion_FloatField:
    return tuix::CreateField(
      builder,
      tuix::FieldUnion_FloatField,
      tuix::CreateFloatField(
        builder,
        static_cast<const tuix::FloatField *>(field->value())->value()).Union(),
      is_null);
  case tuix::FieldUnion_DoubleField:
    return tuix::CreateField(
      builder,
      tuix::FieldUnion_DoubleField,
      tuix::CreateDoubleField(
        builder,
        static_cast<const tuix::DoubleField *>(field->value())->value()).Union(),
      is_null);
  case tuix::FieldUnion_StringField:
  {
    auto string_field = static_cast<const tuix::StringField *>(field->value());
    std::vector<uint8_t> string_data(string_field->value()->begin(),
                                     string_field->value()->end());
    return tuix::CreateField(
      builder,
      tuix::FieldUnion_StringField,
      tuix::CreateStringFieldDirect(
        builder, &string_data, string_field->length()).Union(),
      is_null);
  }
  case tuix::FieldUnion_DateField:
    return tuix::CreateField(
      builder,
      tuix::FieldUnion_DateField,
      tuix::CreateDateField(
        builder,
        static_cast<const tuix::DateField *>(field->value())->value()).Union(),
      is_null);
  default:
    printf("flatbuffers_copy tuix::Field: Unknown field type %d\n",
           field->value_type());
    std::exit(1);
    return flatbuffers::Offset<tuix::Field>();
  }
}
